#include <iostream>
#include <stdio.h>
#include <curl/curl.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <set>

using namespace std;
using namespace rapidjson;
//host with table
//_design/photos/_view/conflict_docs is to fetch
string conflicts_path = "/_design/photos/_view/conflict_docs";
string put_path =  "/_bulk_docs";
const char rows_key[] = "rows";
const char value_key[] = "value";
const char author_key[] = "author";
const char association_key[] = "association";
const char photo_key[] = "photo";
const char associations_key[] = "associations";
const char id_key[] = "id";
const char id_p_key[] = "_id";
const char rev_p_key[] = "_rev";
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

struct DBResponse
{
	string responseBody;
	int status;
};

DBResponse preformGetRequest(string &path)
{
	DBResponse response;
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, path.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.responseBody);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			response.status = -1;
		}
		else {
			long http_code = 0;
			curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
			response.status = http_code;
		}
	}
	else {
		response.status = -1;
	}
	return response;
}

bool preformPostRequest(string &path, const char * body)
{
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	bool no_error = true;
	if (curl) {
		struct curl_slist *headers = NULL;
		headers = curl_slist_append(headers, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,"POST");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
		curl_easy_setopt(curl, CURLOPT_URL, path.c_str());
		res = curl_easy_perform(curl);
		if(res != CURLE_OK){
			cout<<"error: "<<curl_easy_strerror(res)<<endl;
			no_error = false;
		}
		else
		{
			long http_code = 0;
			curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
			if(http_code != 200 && http_code != 201)
			{
				cout<<http_code<<endl;
				no_error = false;
			}
		}
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
	}
	else{
		no_error = false;
	}
	return no_error;
}

int main(int argc, char *argv[]){
	if(argc<2){
		cout<<"Invalid number of arguments"<<endl;
		return 0;
	}
	string fetch_path(argv[1]);
	fetch_path+=conflicts_path;
	cout<<fetch_path<<endl;
	DBResponse response = preformGetRequest(fetch_path);
	if(response.status!=200){
		cout<<"Can't fetch conflicts"<<endl;
		return 0;
	}
	Document dbResponce;
	dbResponce.Parse(response.responseBody.c_str());
	Value& conflicts = dbResponce[rows_key];
	if(!conflicts.IsNull()){
		for (SizeType i = 0; i < conflicts.Size(); i++){
			Value &id =  conflicts[i]["key"];
			string id_value(id.GetString(), id.GetStringLength());
			Value &revisions = conflicts[i][value_key];
			if(!revisions.IsArray()){
				continue;
			}
			Document docs;
			docs.SetObject();
			Value mergedPhoto;
			mergedPhoto.SetObject();
			Document::AllocatorType& allocator = docs.GetAllocator();
			Value associations_out(kArrayType);
			Value docs_out(kArrayType);
			set<string> associations_u;
			for(SizeType j = 0; j < revisions.Size(); j++){
				Value &revision =  revisions[j];
				string rev_value(revision.GetString(), revision.GetStringLength());
				string photo_path(argv[1]);
				photo_path=photo_path+"/"+id_value+"?rev="+rev_value;
				DBResponse version_of_photo = preformGetRequest(photo_path);
				if(response.status != 200){
					cout<<"Can't fetch photo"<<endl;
					continue;
				}
				Document dbResponce;
				dbResponce.Parse(version_of_photo.responseBody.c_str());
				if(j == 0){
					mergedPhoto.AddMember(StringRef(id_p_key),  Value(dbResponce[id_p_key], allocator).Move(),  allocator);
					mergedPhoto.AddMember(StringRef(rev_p_key), Value(dbResponce[rev_p_key], allocator).Move(), allocator);
					mergedPhoto.AddMember(StringRef(author_key), Value(dbResponce[author_key], allocator).Move(), allocator);
					mergedPhoto.AddMember(StringRef(photo_key),  Value(dbResponce[photo_key], allocator).Move(), allocator);

				}
				else{
					Value doc;
					doc.SetObject();
					doc.AddMember(StringRef(id_p_key),  Value(dbResponce[id_p_key], allocator).Move(), allocator);
					doc.AddMember(StringRef(rev_p_key),  Value(dbResponce[rev_p_key], allocator).Move(), allocator);
					Value del;
					del.SetBool(true);
					doc.AddMember("_deleted",  Value(del, allocator).Move(), allocator);
					docs_out.PushBack(Value(doc, allocator).Move(), allocator);
				}
				Value& assocations_rev = dbResponce[associations_key];
				for(SizeType k = 0; k < assocations_rev.Size(); k++) {
					string association_k;
					Value &association = assocations_rev[k];
					string association_value(association[association_key].GetString(), association[association_key].GetStringLength());
					string author_value(association[author_key].GetString(), association[author_key].GetStringLength());
					association_k = association_value + author_value;
					if(associations_u.find(association_k) == associations_u.end()) {
						associations_out.PushBack(Value(association, allocator).Move(), allocator);
						associations_u.insert(association_k);
					}
				}				
			}
			mergedPhoto.AddMember(StringRef(associations_key), Value(associations_out, allocator).Move(), allocator);
			docs_out.PushBack(Value(mergedPhoto, allocator).Move(), allocator);
			docs.AddMember("docs", docs_out, allocator);
			StringBuffer jSonResponce;
			Writer<StringBuffer> writer(jSonResponce);
			docs.Accept(writer);
			string bulk_data_path(argv[1]);
			bulk_data_path = bulk_data_path + put_path;
			if(!preformPostRequest(bulk_data_path, jSonResponce.GetString()))
			{
				cout<<"Can write to db"<<endl;
			}
		}
	}
	else {
		cout<<"Can't send request"<<endl;
	}
	return 0;
}