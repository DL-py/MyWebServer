//
// Created by dl on 3/3/24.
//

#ifndef MYWEBSERVER_HTTP_HANDLER_SIMPLE_H
#define MYWEBSERVER_HTTP_HANDLER_SIMPLE_H
// Linux Header File
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
// C Header File
#include <cstring>
#include <cstdio>
// User Header File
#include "http_com.h"
#include "http_handler.h"

// Linux Header File
#include <sys/stat.h>


class http_handler_simple : public http_handler
{
public:
    static const int FILENAME_LEN = 200;
    static const char* doc_root;
public:
    http_handler_simple(char* url);
    ~http_handler_simple();

public:
    http_com::HTTP_CODE req_handler_pre();
    http_com::HTTP_CODE req_handler();
    http_com::HTTP_CODE req_handler_post();

private:
    char m_real_file[FILENAME_LEN];
    char * m_file_address;
    struct stat m_file_stat;

    http_com::HTTP_CODE m_ret_code;
};


#endif //MYWEBSERVER_HTTP_HANDLER_SIMPLE_H
