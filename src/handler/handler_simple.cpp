#include "handler_simple.h"

#include <cstdlib>
#include <cerrno>
#include <strings.h>

namespace
{
const char* contentTypeForPath(const char* path)
{
    const char* dot = strrchr(path, '.');
    if (dot == nullptr)
    {
        return "application/octet-stream";
    }
    if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0)
    {
        return "text/html; charset=utf-8";
    }
    if (strcasecmp(dot, ".css") == 0)
    {
        return "text/css";
    }
    if (strcasecmp(dot, ".js") == 0 || strcasecmp(dot, ".mjs") == 0)
    {
        return "text/javascript";
    }
    if (strcasecmp(dot, ".json") == 0 || strcasecmp(dot, ".map") == 0)
    {
        return "application/json";
    }
    if (strcasecmp(dot, ".xml") == 0)
    {
        return "application/xml";
    }
    if (strcasecmp(dot, ".svg") == 0)
    {
        return "image/svg+xml";
    }
    if (strcasecmp(dot, ".png") == 0)
    {
        return "image/png";
    }
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0)
    {
        return "image/jpeg";
    }
    if (strcasecmp(dot, ".webp") == 0)
    {
        return "image/webp";
    }
    if (strcasecmp(dot, ".gif") == 0)
    {
        return "image/gif";
    }
    if (strcasecmp(dot, ".ico") == 0)
    {
        return "image/x-icon";
    }
    if (strcasecmp(dot, ".txt") == 0)
    {
        return "text/plain; charset=utf-8";
    }
    if (strcasecmp(dot, ".woff") == 0)
    {
        return "font/woff";
    }
    if (strcasecmp(dot, ".woff2") == 0)
    {
        return "font/woff2";
    }
    if (strcasecmp(dot, ".ttf") == 0)
    {
        return "font/ttf";
    }
    if (strcasecmp(dot, ".pdf") == 0)
    {
        return "application/pdf";
    }
    return "application/octet-stream";
}
}  // namespace

http_handler_simple::http_handler_simple()
    : m_not_found(false),
      m_file_address(nullptr)
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
    std::string urlStorage = req.getURL();
    const size_t queryPos = urlStorage.find('?');
    if (queryPos != std::string::npos)
    {
        urlStorage.erase(queryPos);
    }
    const char* url = urlStorage.c_str();
    int len = static_cast<int>(urlStorage.size());
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
        if (errno != ENOENT)
        {
            resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NO_RESOURCE);
            return false;
        }

        /* fall back to the site's 404 page. */
        std::string notFoundPage = std::string(globalAsset.doc_root) + "/404.html";
        resolvedFile = realpath(notFoundPage.c_str(), NULL);
        if (resolvedFile == NULL)
        {
            resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NO_RESOURCE);
            return false;
        }
        m_not_found = true;
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NO_RESOURCE);
    }
    const std::string realFile = resolvedFile;
    free(resolvedFile);

    const std::string prefix = root + "/";
    if (realFile.compare(0, prefix.size(), prefix) != 0)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::FORBIDDEN_REQUEST);
        return false;
    }

    /* reject paths containing dot-prefixed segments (.git, .DS_Store, ...). */
    {
        const std::string rel = realFile.substr(prefix.size());
        size_t start = 0;
        while (start <= rel.size())
        {
            const size_t slash = rel.find('/', start);
            const std::string segment = rel.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
            if (!segment.empty() && segment[0] == '.')
            {
                resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::FORBIDDEN_REQUEST);
                return false;
            }
            if (slash == std::string::npos)
            {
                break;
            }
            start = slash + 1;
        }
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
        std::string location = req.getURL();
        const size_t queryPos2 = location.find('?');
        if (queryPos2 == std::string::npos)
        {
            location += "/";
        }
        else
        {
            location.insert(queryPos2, "/");
        }
        resp.setHeader("Location", location);
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::MOVED_PERMANENTLY);
        return false;
    }

    resp.setHeader("Content-Type", contentTypeForPath(m_real_file));
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
        if (!m_not_found)
        {
            resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::GOOD_REQUEST);
        }
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
    if (!m_not_found)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::GOOD_REQUEST);
    }
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