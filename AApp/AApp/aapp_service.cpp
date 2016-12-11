#include <fastcgi2/component.h>
#include <fastcgi2/component_factory.h>
#include <fastcgi2/handler.h>
#include <fastcgi2/request.h>
#include <iostream>
#include <stdio.h>
#include <curl/curl.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <regex>
#include <vector>
#include <set>

using namespace std;
using namespace rapidjson;
using namespace fastcgi;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

static vector<string> parse(const std::string& query)
{
	vector<string> associations;
	std::regex pattern("([^=|]+)");
	auto words_begin = sregex_iterator(query.begin(), query.end(), pattern);
	auto words_end = sregex_iterator();
	words_begin++;
	for (std::sregex_iterator i = words_begin; i != words_end; i++) {
		string value = (*i)[1].str();
		associations.push_back(value);
	}
	return associations;
}

struct DBResponse
{
	string responseBody;
	int status;
};

class AAppClass : virtual public Component, virtual public Handler {

public:

	AAppClass(ComponentContext *context) : Component(context) {}

	virtual ~AAppClass() {}

private:
	static const char author_key[];
	static const char association_key[];
	static const char photo_key[];
	static const char associations_key[];
	static const char id_key[];
	static const char id_p_key[];
	static const char value_key[];
	static const char values_key[];
	static const char rows_key[];
	DBResponse requestToDB(bool isPost, const string& address, const StringBuffer* body)
	{
		CURL *curl;
		CURLcode res;
		curl = curl_easy_init();
		DBResponse response;
		response.status = 0;
		if (curl) {
			struct curl_slist *headers = NULL;
			if(isPost)
			{
				headers = curl_slist_append(headers, "Content-Type: application/json");
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,"POST");
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->GetString());
			}
			curl_easy_setopt(curl, CURLOPT_URL, address.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			response.responseBody = "";
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.responseBody);
			res = curl_easy_perform(curl);
			if(res != CURLE_OK){
				cout<<"error: "<<curl_easy_strerror(res)<<endl;
				response.status = 400;
			}
			else
			{
				long http_code = 0;
				curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
				if(http_code != 200 && http_code != 201)
				{
					cout<<http_code<<endl;
					response.status = 404;
				}
			}
			if(isPost)
			{
				curl_slist_free_all(headers);
			}
			curl_easy_cleanup(curl);
		}
		else{
			response.status = 500;
		}
		return response;
	}

	void getSinglePhoto(fastcgi::Request *request, fastcgi::HandlerContext *context, const string& photo_link){
		CURL *curl;
		CURLcode res;
		curl = curl_easy_init();
		DBResponse response = requestToDB(false, "http://127.0.0.1:5984/photos/"+photo_link, nullptr);
		if(response.status!=0) {
			request->sendError(response.status);
		}
		else{
			Document dbResponce;
			dbResponce.Parse(response.responseBody.c_str());
			Document serverResponce;
			serverResponce.SetObject();
			Document::AllocatorType& allocator = serverResponce.GetAllocator();
			serverResponce.AddMember(StringRef(id_key), dbResponce[id_p_key], allocator);
			serverResponce.AddMember(StringRef(photo_key), dbResponce[photo_key], allocator);
			serverResponce.AddMember(StringRef(author_key), dbResponce[author_key], allocator);
			serverResponce.AddMember(StringRef(associations_key), dbResponce[associations_key], allocator);
			StringBuffer jSonResponce;
			Writer<StringBuffer> writer(jSonResponce);
			serverResponce.Accept(writer);
			request->write(jSonResponce.GetString(), jSonResponce.GetSize());
			request->setStatus(200);
		}
	}

	void getPhotosByAssociations(fastcgi::Request *request, fastcgi::HandlerContext *context, vector<string> &associations){
		CURL *curl;
		CURLcode res;
		curl = curl_easy_init();
		Document keysDocument;
		keysDocument.SetObject();
		Value associations_keys(kArrayType);
		Document::AllocatorType& allocator = keysDocument.GetAllocator();
		for(vector<string>::iterator it = associations.begin(); it!=associations.end(); it++)
		{
			Value key;
			key.SetString(StringRef((*it).c_str()));
			associations_keys.PushBack(key, allocator);
		}
		keysDocument.AddMember("keys", associations_keys, allocator);
		StringBuffer buffer;
		Writer<StringBuffer> writer(buffer);
		keysDocument.Accept(writer);
		DBResponse response = requestToDB(true, "http://localhost:5984/photos/_design/photos/_view/by_association", &buffer);
		if(response.status!=0) {
			request->sendError(response.status);
		}
		else{
			Document dbResponce;
			dbResponce.Parse(response.responseBody.c_str());
			Document serverResponce;
			serverResponce.SetObject();
			Document::AllocatorType& allocator = serverResponce.GetAllocator();
			Value& photos = dbResponce[rows_key];
			Value photos_out(kArrayType);
			if(!photos.IsNull()){
				set<string> ids;
				for (SizeType i = 0; i < photos.Size(); i++){
						Value &id =  photos[i][id_key];
						string id_value(id.GetString(), id.GetStringLength());
						if(ids.find(id_value)!=ids.end()){
							continue;
						}
						ids.insert(id_value);
						Value& original_photo =  photos[i][value_key];
						Value photo(kObjectType);
						photo.AddMember(StringRef(id_key), original_photo[id_p_key], allocator);
						photo.AddMember(StringRef(photo_key), original_photo[photo_key], allocator);
						photo.AddMember(StringRef(author_key), original_photo[author_key], allocator);
						photo.AddMember(StringRef(associations_key),original_photo[associations_key], allocator);
						photos_out.PushBack(photo, allocator);
				} 
			}
			serverResponce.AddMember(StringRef(values_key), photos_out, allocator);
			StringBuffer jSonResponce;
			Writer<StringBuffer> writer(jSonResponce);
			serverResponce.Accept(writer);
			request->write(jSonResponce.GetString(), jSonResponce.GetSize());
			request->setStatus(200);
		}
	}

	void postSinglePhotoFromJSon(fastcgi::Request *request, fastcgi::HandlerContext *context){
		DataBuffer postBody = request->requestBody();
		if(postBody.empty())
		{
			cout<<"No body"<<endl;
			request->sendError(400);
			return;
		}
		string jSonFile;
		postBody.toString(jSonFile);
		//const char* json = "{\"photo\":\"photo1\",\"author\":\"John Gold\", \"associations\":[\"hist1\", \"hist2\"]}";
		Document requestBody;
		requestBody.Parse(jSonFile.c_str());
		Document photoDocument;
		photoDocument.SetObject();
		Value associations_out(kArrayType);
		Document::AllocatorType& allocator = photoDocument.GetAllocator();
		Value& photo = requestBody[photo_key];
		if(photo.IsNull()){
			request->sendError(400);
			return;
		}
		Value& author  = requestBody[author_key];
		if(author.IsNull()){
			request->sendError(400);
			return;
		}
		Value& associations = requestBody[associations_key];
		if(associations.IsNull()){
			request->sendError(400);
			return;
		}
		if(associations.IsArray()) {
			for (SizeType i = 0; i < associations.Size(); i++){
				if(associations[i].IsString()) {
					Value association_author(kObjectType);
					association_author.AddMember(StringRef(author_key), Value(author, allocator).Move(), allocator);
					association_author.AddMember(StringRef(association_key), associations[i], allocator);
					associations_out.PushBack(association_author, allocator);
				} else {
					request->sendError(400);
					return;
				}
			}
		}
		else {
			request->sendError(400);
			return;
		}
		photoDocument.AddMember(StringRef(photo_key), photo, allocator);
		photoDocument.AddMember(StringRef(author_key), Value(author, allocator).Move(), allocator);
		photoDocument.AddMember(StringRef(associations_key), associations_out, allocator);
		StringBuffer buffer;
		Writer<StringBuffer> writer(buffer);
		photoDocument.Accept(writer);
		DBResponse response = requestToDB(true, "http://127.0.0.1:5984/photos", &buffer);
		if(response.status!=0) {
			request->sendError(response.status);
		}
		else{
			Document dbResponce;
			dbResponce.Parse(response.responseBody.c_str());
			Document serverResponce;
			serverResponce.SetObject();
			Document::AllocatorType& allocator = serverResponce.GetAllocator();
			serverResponce.AddMember(StringRef(id_key), dbResponce[id_key], allocator);
			StringBuffer jSonResponce;
			Writer<StringBuffer> writer(jSonResponce);
			serverResponce.Accept(writer);
			request->write(jSonResponce.GetString(), jSonResponce.GetSize());
			request->setStatus(201);
		}
	}

	void postAssociationsForPhotoFromJSon(fastcgi::Request *request, fastcgi::HandlerContext *context, const string& photo_link){
		DataBuffer postBody = request->requestBody();
		if(postBody.empty())
		{
			cout<<"No body"<<endl;
			request->sendError(400);
			return;
		}
		string jSonFile;
		postBody.toString(jSonFile);
		//const char* json = "{\"photo\":\"photo1\",\"author\":\"John Gold\", \"associations\":[\"hist1\", \"hist2\"]}";
		Document requestBody;
		requestBody.Parse(jSonFile.c_str());
		Document photoDocument;
		photoDocument.SetObject();
		Value& author = requestBody[author_key];
		if(author.IsNull()){
			request->sendError(400);
			return;
		}
		Value& associations = requestBody[associations_key];
		if(associations.IsNull()){
			request->sendError(400);
			return;
		}
		Document associationsDocument;
		associationsDocument.SetObject();
		Value associations_out(kArrayType);
		Document::AllocatorType& allocator = associationsDocument.GetAllocator();
		if(associations.IsArray()) {
			for (SizeType i = 0; i < associations.Size(); i++){
				if(associations[i].IsString()) {
					Value association_author(kObjectType);
					association_author.AddMember(StringRef(author_key), Value(author, allocator).Move(), allocator);
					association_author.AddMember(StringRef(association_key), associations[i], allocator);
					associations_out.PushBack(association_author, allocator);
				} else {
					request->sendError(400);
					return;
				}
			}
		}
		else {
			request->sendError(400);
			return;
		}
		associationsDocument.AddMember(StringRef(associations_key), associations_out, allocator);
		CURL *curl;
		CURLcode res;
		curl = curl_easy_init();
		StringBuffer buffer;
		Writer<StringBuffer> writer(buffer);
		associationsDocument.Accept(writer);
		DBResponse response = requestToDB(true, "http://127.0.0.1:5984/photos/_design/photos/_update/add_association/"+photo_link, &buffer);
		if(response.status!=0) {
			request->sendError(response.status);
		}
		else{
			Document dbResponce;
			dbResponce.Parse(response.responseBody.c_str());
			Document serverResponce;
			serverResponce.SetObject();
			Document::AllocatorType& allocator = serverResponce.GetAllocator();
			serverResponce.AddMember(StringRef(id_key), dbResponce[id_p_key], allocator);
			serverResponce.AddMember(StringRef(photo_key), dbResponce[photo_key], allocator);
			serverResponce.AddMember(StringRef(author_key), dbResponce[author_key], allocator);
			serverResponce.AddMember(StringRef(associations_key), dbResponce[associations_key], allocator);
			StringBuffer jSonResponce;
			Writer<StringBuffer> writer(jSonResponce);
			serverResponce.Accept(writer);
			request->write(jSonResponce.GetString(), jSonResponce.GetSize());
			request->setStatus(201);
		}
	}

public:

	virtual void onLoad() {}

	virtual void onUnload() {}

	virtual void handleRequest(fastcgi::Request *request, fastcgi::HandlerContext *context) {
		string methodHttp = request->getRequestMethod();
		string get_photo;
		string uri = request->getURI();
		if(methodHttp=="GET"){
			regex get_photos_associations("^/aapp/photos\\?associations=[^|]+([|][^|]+)*$");
			if(regex_match (uri, get_photos_associations)){
				vector<string> associations =  parse(request->getQueryString());
				getPhotosByAssociations(request, context, associations);
				return;
			}
			regex get_photo("^/aapp/photos/([^/]+)$");
			smatch sm;
			if (regex_search(uri, sm, get_photo)) {
				getSinglePhoto(request,context, sm[1]);
				return;
			}
			else {
				request->sendError(404);
			}
			return;
		}
		else if(methodHttp=="POST")
		{
			regex post_photo("^/aapp/photos$");
			regex post_association("^/aapp/photos/([^/]+)/associations$");
			if(regex_match (uri, post_photo)){
				postSinglePhotoFromJSon(request, context);
				return;
			}
			else {
				smatch sm;
				if(regex_search(uri, sm, post_association)){
					postAssociationsForPhotoFromJSon(request, context, sm[1]);
					return;
				}
				else {
					request->sendError(404);
				}
			}
		}
		else {
			request->sendError(404);
		}
	}
};

const char AAppClass::author_key[] = "author";
const char AAppClass::association_key[] = "association";
const char AAppClass::photo_key[] = "photo";
const char AAppClass::associations_key[] = "associations";
const char AAppClass::id_key[] = "id";
const char AAppClass::id_p_key[] = "_id";
const char AAppClass::value_key[] = "value";
const char AAppClass::values_key[] = "values";
const char AAppClass::rows_key[] = "rows";
FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
FCGIDAEMON_ADD_DEFAULT_FACTORY("simple_factory", AAppClass)
FCGIDAEMON_REGISTER_FACTORIES_END()