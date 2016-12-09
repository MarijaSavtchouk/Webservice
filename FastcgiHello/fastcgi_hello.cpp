#include <fastcgi2/component.h>
#include <fastcgi2/component_factory.h>
#include <fastcgi2/handler.h>
#include <fastcgi2/request.h>
#include <iostream>
#include <stdio.h>
#include <curl/curl.h>

using namespace std;

class HelloClass : virtual public fastcgi::Component, virtual public fastcgi::Handler {

public:

	HelloClass(fastcgi::ComponentContext *context) : fastcgi::Component(context) {}

	virtual ~HelloClass() {}

public:
	virtual void onLoad() {}
	virtual void onUnload() {}
	virtual void handleRequest(fastcgi::Request *request, fastcgi::HandlerContext *context) {
		 request->setHeader("Simple-Header", "Reply hello");
		 string str = "Hello, world!\n";
		 request->write(str.c_str(), str.length());
	}
};

FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
FCGIDAEMON_ADD_DEFAULT_FACTORY("simple_factory", HelloClass)
FCGIDAEMON_REGISTER_FACTORIES_END()
