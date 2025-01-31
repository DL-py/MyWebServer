//
// Created by dl on 2/29/24.
//
#include "http_resp.h"
/**
 * status line:HTTP/1.1 200 OK\r\n
 * header:Content-Length: 
 *
 * */

http_resp::http_resp():m_buffer(nullptr), m_buffer_idx(0), m_buffer_size(0)
{

}

http_resp::~http_resp()
{

}

void http_resp::init(char* buffer, int buffer_size)
{
    m_buffer = buffer;
    m_buffer_idx = 0;
    m_buffer_size = buffer_size;
}

void http_resp::reset(char* buffer, int buffer_size)
{
    m_buffer = buffer;
    m_buffer_idx = 0;
    m_buffer_size = buffer_size;
}

bool http_resp::add_response( const char* format, ... )
{
    if( m_buffer_idx >= m_buffer_size )
    {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_buffer + m_buffer_idx, m_buffer_size - 1 - m_buffer_idx, format, arg_list );
    if( len >= ( m_buffer_size - 1 - m_buffer_idx ) )
    {
        return false;
    }
    m_buffer_idx += len;
    va_end( arg_list );
    return true;
}

bool http_resp::add_status_line( int status, const char* title )
{
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

bool http_resp::add_content_length( int content_len )
{
    return add_response( "Content-Length: %d\r\n", content_len );
}

bool http_resp::add_linger(bool linger)
{
    return add_response( "Connection: %s\r\n", ( linger == true ) ? "keep-alive" : "close" );
}

bool http_resp::add_blank_line()
{
    return add_response( "%s", "\r\n" );
}

bool http_resp::add_content( const char* content )
{
    return add_response( "%s", content );
}

bool http_resp::add_headers( int content_len, bool linger )
{
    bool ret = false;
    ret = add_content_length( content_len );
    ret = add_linger(linger);
    ret = add_blank_line();
    return ret;
}

void http_resp::set_buffer(char* buffer)
{
    m_buffer = buffer;
}
char* http_resp::get_buffer()
{
    return m_buffer;
}
void http_resp::set_buffer_idx(int buffer_idx)
{
    m_buffer_idx = buffer_idx;
}
int http_resp::get_buffer_idx()
{
    return m_buffer_idx;
}
void http_resp::set_buffer_size(int buffer_size)
{
    m_buffer_size = buffer_size;
}

int http_resp::get_buffer_size()
{
    return m_buffer_size;
}
