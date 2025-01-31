//
// Created by dl on 3/14/24.
//

#ifndef MYWEBSERVER_HTTP_HANDLER_CGI_H
#define MYWEBSERVER_HTTP_HANDLER_CGI_H
// Linux Header File
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
// C Header Files
#include <cstdio>
#include <cstring>
#include <cstdlib>
//User Header Files
#include "http_handler.h"

class http_handler_cgi : public http_handler
{
public:
    static const int FILENAME_LEN = 200;
    static const int MAX_BUFFER_SIZE = 1024;
    static const int MAX_HANDLER_BUFFER_SIZE = 4096;
    static const char* doc_root;
public:
    http_handler_cgi(char* url, char* content, int content_length);
    ~http_handler_cgi();

public:
    http_com::HTTP_CODE req_handler_pre();
    http_com::HTTP_CODE req_handler();
    http_com::HTTP_CODE req_handler_post();

private:
    char m_real_file[FILENAME_LEN];
    char * m_file_address;
    struct stat m_file_stat;

    char* m_content;
    int m_content_len;

    http_com::HTTP_CODE m_ret_code;
};


#endif //MYWEBSERVER_HTTP_HANDLER_CGI_H
