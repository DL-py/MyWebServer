//
// Created by dl on 3/14/24.
//
#include "http_handler_factory.h"
http_handler* http_handler_produce(http_com::HTTP_CODE http_code, http_com::METHOD method, char* url, char* content, int content_len)
{
    http_handler* handler = nullptr;
    if (method == http_com::GET)
    {
        handler = new http_handler_simple(url);
    }
    else if(method == http_com::POST)
    {
        handler = new http_handler_cgi(url, content, content_len);
    }
    else
    {
        return handler;
    }
    return handler;
}


void http_handler_destruct(http_handler* handler)
{
    delete handler;
}