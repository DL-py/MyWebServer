//
// Created by dl on 3/2/24.
//

#ifndef MYWEBSERVER_HTTP_REQ_H
#define MYWEBSERVER_HTTP_REQ_H
// C++ Header File
#include <cstring>
#include <iostream>
// User Header File
#include "http_com.h"

class http_req
{
public:
    http_req();
    ~http_req();

public:
    void init(char* read_buf, int buffer_size);
    void reset(char* read_buf, int buffer_size);

    http_com::LINE_STATUS parse_line();
    http_com::HTTP_CODE parse_request_line( char* text );
    http_com::HTTP_CODE parse_headers( char* text );
    http_com::HTTP_CODE parse_content( char* text );

public:
    char * get_line() {return m_read_buf + m_start_line;}
    bool is_renewable() {return m_read_idx < m_buffer_size;}
    char* get_read_pos() {return m_read_buf + m_read_idx;}
    void update_read_pos(int nbyte_inc) {m_read_idx += nbyte_inc;}
    int get_checked_idx() {return m_checked_idx;}
    void set_start_line(int idx) {m_start_line = idx;}
    int nbytes_left() {return m_buffer_size - m_read_idx;}
    bool get_header_linger() {return m_linger;}
    int get_header_content_length() {return m_content_length;}

private:
    char* m_read_buf;
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    int m_buffer_size;

public:
    // http request header:
    char * m_url;
    char * m_version;
    char * m_host;
    int  m_content_length;
    bool m_linger;
    http_com::METHOD m_method;
    
    // http request content:
    char* m_content;
};
#endif //MYWEBSERVER_HTTP_REQ_H
