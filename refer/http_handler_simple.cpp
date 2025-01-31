//
// Created by dl on 3/3/24.
//

#include "http_handler_simple.h"

const char* http_handler_simple::doc_root = "html";

http_handler_simple::http_handler_simple(char* url): http_handler()
{
    
}

http_handler_simple::~http_handler_simple()
{

}

http_com::HTTP_CODE http_handler_simple::req_handler_pre()
{
    if (m_ret_code != http_com::GET_REQUEST)
    {
        return m_ret_code;
    }

    if ( stat( m_real_file, &m_file_stat ) < 0 )
    {
        m_ret_code = http_com::NO_RESOURCE;
        return m_ret_code;
    }

    if ( ! ( m_file_stat.st_mode & S_IROTH ) )
    {
        m_ret_code = http_com::FORBIDDEN_REQUEST;
        return m_ret_code;
    }

    if ( S_ISDIR( m_file_stat.st_mode ) )
    {
        m_ret_code =  http_com::BAD_REQUEST;
        return m_ret_code;
    }

    return m_ret_code;
}


http_com::HTTP_CODE http_handler_simple::req_handler()
{
    int fd = open( m_real_file, O_RDONLY );
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );

    m_handler_buf = m_file_address;
    m_handler_buf_len = m_file_stat.st_size;

    close( fd );
    m_ret_code = http_com::FILE_REQUEST;
    return m_ret_code;
}

http_com::HTTP_CODE http_handler_simple::req_handler_post()
{
    if( m_file_address )
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = nullptr;
    }
    return m_ret_code;
}

