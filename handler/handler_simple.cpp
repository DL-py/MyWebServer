#include "handler_simple.h"

#include <cstdlib>

http_handler_simple::http_handler_simple()
    : m_file_address(nullptr)
{
    m_real_file[0] = '\0';
    memset(&m_file_stat, 0, sizeof(m_file_stat));
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

    if (len == 0)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return false;
    }

    int written = 0;
    if (strcmp(url, "/") == 0)
    {
        written = snprintf(m_real_file, FILENAME_LEN, "%s/%s", globalAsset.doc_root, globalAsset.default_page);
    }
    else if (url[len-1] == '/')
    {
        written = snprintf(m_real_file, FILENAME_LEN, "%s/%s%s", globalAsset.doc_root, url, globalAsset.default_page);
    }
    else 
    {
        written = snprintf(m_real_file, FILENAME_LEN, "%s%s", globalAsset.doc_root, url);
    }
    if (written < 0 || written >= FILENAME_LEN)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return false;
    }

    char* resolvedRoot = realpath(globalAsset.doc_root, NULL);
    if (resolvedRoot == NULL)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::INTERNAL_ERROR);
        return false;
    }
    const std::string root = resolvedRoot;
    free(resolvedRoot);

    char* resolvedFile = realpath(m_real_file, NULL);
    if (resolvedFile == NULL)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NO_RESOURCE);
        return false;
    }
    const std::string realFile = resolvedFile;
    free(resolvedFile);

    const std::string prefix = root + "/";
    if (realFile.compare(0, prefix.size(), prefix) != 0)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::FORBIDDEN_REQUEST);
        return false;
    }

    snprintf(m_real_file, FILENAME_LEN, "%s", realFile.c_str());
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
    if (fd < 0)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NO_RESOURCE);
        return false;
    }

    if (m_file_stat.st_size == 0)
    {
        resp.setBody("");
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::GOOD_REQUEST);
        close(fd);
        return true;
    }

    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close(fd);
    if (m_file_address == MAP_FAILED)
    {
        m_file_address = nullptr;
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::INTERNAL_ERROR);
        return false;
    }

    resp.setBody(std::string(m_file_address, m_file_stat.st_size));
    resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::GOOD_REQUEST);
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