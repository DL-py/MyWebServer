//
// Created by dl on 2/29/24.
//

#ifndef MYWEBSERVER_HTTP_RESP_H
#define MYWEBSERVER_HTTP_RESP_H
// C++ Header File
#include <iostream>
#include <vector>
// C Header File
#include <cstdarg>
#include <cstring>
// User Header File
#include "http_com.h"

using namespace std;

class http_resp
{
public:
    http_resp();
    ~http_resp();
    void init(char* buffer, int buffer_size);
    void reset(char* buffer, int buffer_size);

    bool add_status_line( int status, const char* title );
    virtual bool add_headers( int content_len, bool linger );
    bool add_blank_line();
    bool add_content( const char* content );
    bool add_linger(bool linger);
    bool add_content_length( int content_len );
    bool add_response(const char* format, ...);

public:
    int get_nbytes_buffer() {return m_buffer_idx;}
    void set_buffer(char* buffer);
    char* get_buffer();
    void set_buffer_idx(int buffer_idx);
    int get_buffer_idx();
    void set_buffer_size(int buffer_size);
    int get_buffer_size();

private:
    char* m_buffer;
    int m_buffer_idx;
    int m_buffer_size;
};
#endif //MYWEBSERVER_HTTP_RESP_H
