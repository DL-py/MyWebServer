#include "handler_simple.h"


http_handler_simple::http_handler_simple()
{

}

http_handler_simple::~http_handler_simple()
{

}

bool http_handler_simple::handler_pre(HttpRequest& req, HttpResponse& resp)
{
    /* construct m_real_file. */
    const char* url = req.getURL().c_str();
    int len = strlen(url);
    LogRecord(logger, LogLevel::LOG_DEBUG, "request url: %s.\n", url);

    if (strcmp(url, "/") == 0)
    {
        snprintf(m_real_file, FILENAME_LEN, "%s/%s", globalAsset.doc_root, globalAsset.default_page);
    }
    else if (url[len-1] == '/')
    {
        snprintf(m_real_file, FILENAME_LEN, "%s/%s%s", globalAsset.doc_root, url, globalAsset.default_page);
    }
    else 
    {
        snprintf(m_real_file, FILENAME_LEN, "%s%s", globalAsset.doc_root, url);
    }
    
    if (stat( m_real_file, &m_file_stat ) < 0 )
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NO_RESOURCE);
        return false;
    }

    if ( ! ( m_file_stat.st_mode & S_IROTH ) )
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::FORBIDDEN_REQUEST);
        return false;
    }

    if ( S_ISDIR( m_file_stat.st_mode ) )
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return false;
    }

    return true;
}

bool http_handler_simple::handler(HttpRequest& req, HttpResponse& resp)
{
    int fd = open( m_real_file, O_RDONLY );
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );

    resp.setBody(std::string(m_file_address, m_file_stat.st_size));
    resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::GOOD_REQUEST);
    
    close(fd);
    return true;
}

bool http_handler_simple::handler_post(HttpRequest& req, HttpResponse& resp)
{
    if( m_file_address )
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = nullptr;
    }
    return false;
}