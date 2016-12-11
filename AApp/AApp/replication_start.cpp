#include <iostream>
#include <stdio.h>
#include <curl/curl.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <fstream>

using namespace std;
using namespace rapidjson;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}
//sourse, target, host optional flag if not continuous
int main(int argc, char *argv[]){
	if(argc<4){
		cout<<"Invalid number of arguments"<<endl;
		return 0;
	}
	Document replication;
	replication.SetObject();
	Document::AllocatorType& allocator = replication.GetAllocator();
	Value node;
	node.SetString(argv[1], strlen(argv[1]), allocator);
	replication.AddMember("source", node, allocator);
	node.SetString(argv[2], strlen(argv[2]), allocator);
	replication.AddMember("target", node, allocator);
	if(argc!=5)
	{
		node.SetBool(true);
		replication.AddMember("continuous", node, allocator);
	}
	StringBuffer jSonBody;
	Writer<StringBuffer> writer(jSonBody);
	replication.Accept(writer);
	cout<<jSonBody.GetString()<<endl;
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if (curl) {
		struct curl_slist *headers = NULL;
		headers = curl_slist_append(headers, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,"POST");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jSonBody.GetString());
		curl_easy_setopt(curl, CURLOPT_URL, (string(argv[3])+"/_replicate").c_str());
		string responseInfo;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseInfo);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			cout<<"Can't send request"<<endl;
		}
		else
		{
			long http_code = 0;
			curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
			if(http_code != 202 && http_code!=200){
				cout<<"Faild to create replication document"<<http_code<<endl;
			}
			else{
				if(argc==5){
					cout<<"Information about replication"<<endl;
					cout<<responseInfo.c_str()<<endl;
				}
				else{
					Document dbResponce;
					dbResponce.Parse(responseInfo.c_str());
					ofstream replication_info_file("replication_info");
					replication_info_file<<"Use it to cancel replication"<<endl;
					replication_info_file<<"replication_id: "<<dbResponce["_local_id"].GetString()<<endl;
					replication_info_file.close();
				}
			}
		}
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
	}
	else{
		cout<<"Can't send request"<<endl;
	}

	return 0;
}
/*USAGE  curl -H 'Content-Type: application/json' -X POST http://localhost:5984/_replicate -d '{"replication_id": "b25244d4887b574d9a6cbd70974d7dd7+continuous", "cancel": true}'*/