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
#include "handler.h"
// Linux Header File
#include <sys/stat.h>


class http_handler_simple : public http_handler
{
public:
    static const int FILENAME_LEN = 1024;
    static const char* doc_root;
public:
    http_handler_simple();
    ~http_handler_simple();

    bool handler_pre(HttpRequest& req, HttpResponse& resp) override;
    bool handler(HttpRequest& req, HttpResponse& resp) override;
    bool handler_post(HttpRequest& req, HttpResponse& resp) override;

private:
    char m_real_file[FILENAME_LEN];
    char * m_file_address;
    struct stat m_file_stat;

    // http_com::HTTP_CODE m_ret_code;
};


#endif