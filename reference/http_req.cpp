//
// Created by dl on 3/2/24.
//
#include "http_req.h"
http_req::http_req():m_read_buf(nullptr), m_read_idx(0), m_checked_idx(0), m_start_line(0), m_buffer_size(0)
{
    m_url = nullptr;
    m_version = nullptr;
    m_host = nullptr;
    m_content_length = 0;
    m_linger = false;
    m_method = http_com::GET;

    m_content = nullptr;
}

http_req::~http_req()
{

}

void http_req::init(char* read_buf, int buffer_size)
{
    m_read_buf = read_buf;
    m_buffer_size = buffer_size;
}

void http_req::reset(char* read_buf, int buffer_size)
{
    m_read_buf = read_buf;
    m_read_idx = 0;
    m_checked_idx = 0;
    m_start_line = 0;
    m_buffer_size = buffer_size;

    m_url = nullptr;
    m_version = nullptr;
    m_host = nullptr;
    m_content_length = 0;
    m_linger = false;
    m_method = http_com::GET;
    
    m_content = nullptr;
}

http_com::LINE_STATUS http_req::parse_line()
{
    char temp;
    for ( ; m_checked_idx < m_read_idx; ++m_checked_idx )
    {
        temp = m_read_buf[ m_checked_idx ];
        if ( temp == '\r' )
        {
            if ( ( m_checked_idx + 1 ) == m_read_idx )
            {
                return http_com::LINE_OPEN;
            }
            else if ( m_read_buf[ m_checked_idx + 1 ] == '\n' )
            {
                m_read_buf[ m_checked_idx++ ] = '\0';
                m_read_buf[ m_checked_idx++ ] = '\0';
                return http_com::LINE_OK;
            }

            return http_com::LINE_BAD;
        }
        else if( temp == '\n' )
        {
            if( ( m_checked_idx > 1 ) && ( m_read_buf[ m_checked_idx - 1 ] == '\r' ) )
            {
                m_read_buf[ m_checked_idx-1 ] = '\0';
                m_read_buf[ m_checked_idx++ ] = '\0';
                return http_com::LINE_OK;
            }
            return http_com::LINE_BAD;
        }
    }

    return http_com::LINE_OPEN;
}

http_com::HTTP_CODE http_req::parse_request_line( char* text )
{
    m_url = strpbrk( text, " \t" );
    if ( ! m_url )
    {
        return http_com::BAD_REQUEST;
    }
    *m_url++ = '\0';

    char* method = text;
    if ( strcasecmp( method, "GET" ) == 0 )
    {
        m_method = http_com::GET;
    }
    else if( strcasecmp(method, "POST") == 0)
    {
        m_method = http_com::POST;
    }
    else
    {
        return http_com::BAD_REQUEST;
    }

    m_url += strspn( m_url, " \t" );
    m_version = strpbrk( m_url, " \t" );
    if ( ! m_version )
    {
        return http_com::BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn( m_version, " \t" );
    if ( strcasecmp( m_version, "HTTP/1.1" ) != 0 )
    {
        return http_com::BAD_REQUEST;
    }
    /* 如果请求行中没有http:// 比如请求行直接为 GET /test.html HTTP/1.1 */
    /* 此时，m_url = "/test.html" */
    if ( strncasecmp( m_url, "http://", 7 ) == 0 )
    {
        m_url += 7;
        m_url = strchr( m_url, '/' );
    }

    if ( ! m_url || m_url[ 0 ] != '/' )
    {
        return http_com::BAD_REQUEST;
    }

    return http_com::NO_REQUEST;
}

http_com::HTTP_CODE http_req::parse_headers( char* text )
{
    if ( strncasecmp( text, "Connection:", 11 ) == 0 )
    {
        text += 11;
        text += strspn( text, " \t" );
        if ( strcasecmp( text, "keep-alive" ) == 0 )
        {
            m_linger = true;
        }
    }
    else if ( strncasecmp( text, "Content-Length:", 15 ) == 0 )
    {
        text += 15;
        text += strspn( text, " \t" );
        m_content_length = atol( text );
    }
    else if ( strncasecmp( text, "Host:", 5 ) == 0 )
    {
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
    }
    else
    {
        //printf( "oop! unknow header %s\n", text );
    }

    return http_com::NO_REQUEST;
}

http_com::HTTP_CODE http_req::parse_content( char* text )
{
    if ( m_read_idx >= ( m_content_length + m_checked_idx ) )
    {
        text[ m_content_length ] = '\0';
        m_content = text;
        return http_com::GET_REQUEST;
    }

    return http_com::NO_REQUEST;
}
