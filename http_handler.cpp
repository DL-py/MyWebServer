//
// Created by dl on 3/3/24.
//

#include "http_handler.h"
http_handler::http_handler():m_handler_buf(nullptr), m_handler_buf_len(0)
{

}

http_handler::~http_handler()
{

}

char* http_handler::get_handler_buffer()
{
    return m_handler_buf;
}

int http_handler::get_handler_buffer_len()
{
    return m_handler_buf_len;
}