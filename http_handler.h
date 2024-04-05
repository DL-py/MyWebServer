//
// Created by dl on 3/3/24.
//

#ifndef MYWEBSERVER_HTTP_HANDLER_H
#define MYWEBSERVER_HTTP_HANDLER_H
// User Header File
#include "http_com.h"

class http_handler
{
public:
    virtual http_com::HTTP_CODE req_handler_pre() = 0;
    virtual http_com::HTTP_CODE req_handler() = 0;
    virtual http_com::HTTP_CODE req_handler_post() = 0;
    http_handler();
    virtual ~http_handler() = 0;

protected:
    char* m_handler_buf;
    int m_handler_buf_len;

public:
    char* get_handler_buffer();
    int get_handler_buffer_len();
};


#endif //MYWEBSERVER_HTTP_HANDLER_H
