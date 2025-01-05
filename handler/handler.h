#ifndef MYWEBSERVER_HTTP_HANDLER_H
#define MYWEBSERVER_HTTP_HANDLER_H
#include "../http_messages/http_request.h"
#include "../http_messages/http_response.h"

class http_handler
{

public:
    http_handler();
    ~http_handler();
    virtual bool handler_pre(HttpRequest& req, HttpResponse& resp) = 0;
    virtual bool handler(HttpRequest& req, HttpResponse& resp) = 0;
    virtual bool handler_post(HttpRequest& req, HttpResponse& resp) = 0;

protected:
};


#endif

