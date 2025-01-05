#ifndef  ROUTER_H
#define  ROUTER_H
#include <cassert>
#include <string>
#include <unordered_map>
#include "../handler/handler.h"
#include "../handler/handler_simple.h"
#include "../http_messages/http_request.h"
#include "../http_messages/http_response.h"

class Router
{
public:
    static http_handler* route(HttpRequest& req, HttpResponse& resp);

};



#endif
