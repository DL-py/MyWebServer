//
// Created by dl on 3/14/24.
//
#ifndef MYWEBSERVER_HTTP_HANDLER_FACTORY_H
#define MYWEBSERVER_HTTP_HANDLER_FACTORY_H
// User Header Files
#include "http_com.h"
#include "http_handler.h"
#include "http_handler_simple.h"
#include "http_handler_cgi.h"

http_handler* http_handler_produce(http_com::HTTP_CODE http_code, http_com::METHOD method, char* url, char* content, int content_len);
void http_handler_destruct(http_handler* handler);

#endif //MYWEBSERVER_HTTP_HANDLER_FACTORY_H
