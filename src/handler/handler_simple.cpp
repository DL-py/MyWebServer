#include "handler_simple.h"

#include <cstdlib>
#include <cerrno>
#include <ctime>
#include <strings.h>

#include "../http_utils/etag_cache.h"
#include "../http_utils/gzip.hpp"
#include "../http_utils/http_protocol.hpp"

namespace
{
/* files above this size are served identity: readWholeFile buffers them and
   zlib length fields are 32-bit. */
const std::size_t kGzipMaxInput = 32 * 1024 * 1024;

bool etagMatches(const std::string& header, const std::string& etag)
{
    std::string trimmed = header;
    while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t'))
    {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() &&
           (trimmed[trimmed.size() - 1] == ' ' || trimmed[trimmed.size() - 1] == '\t'))
    {
        trimmed.erase(trimmed.size() - 1);
    }
    if (trimmed == "*")
    {
        return true;
    }
    if (etag.empty())
    {
        return false;
    }

    const std::string normalized = etag;
    size_t start = 0;
    while (start <= trimmed.size())
    {
        const size_t comma = trimmed.find(',', start);
        std::string token = trimmed.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        while (!token.empty() && (token[0] == ' ' || token[0] == '\t'))
        {
            token.erase(token.begin());
        }
        while (!token.empty() && (token[token.size() - 1] == ' ' || token[token.size() - 1] == '\t'))
        {
            token.erase(token.size() - 1);
        }
        if (token.compare(0, 2, "W/") == 0 || token.compare(0, 2, "w/") == 0)
        {
            token.erase(0, 2);
        }
        if (token == normalized)
        {
            return true;
        }
        if (comma == std::string::npos)
        {
            break;
        }
        start = comma + 1;
    }
    return false;
}

bool isCompressibleType(const char* contentType)
{
    return strcasecmp(contentType, "text/html; charset=utf-8") == 0 ||
           strcasecmp(contentType, "text/css") == 0 ||
           strcasecmp(contentType, "text/javascript") == 0 ||
           strcasecmp(contentType, "text/plain; charset=utf-8") == 0 ||
           strcasecmp(contentType, "application/json") == 0 ||
           strcasecmp(contentType, "application/xml") == 0 ||
           strcasecmp(contentType, "application/manifest+json") == 0 ||
           strcasecmp(contentType, "image/svg+xml") == 0;
}

std::string gzipVariant(const std::string& etag)
{
    if (etag.size() < 2 || etag[etag.size() - 1] != '"')
    {
        return "";
    }
    return etag.substr(0, etag.size() - 1) + "-gzip\"";
}

long long fileStamp(const struct stat& st)
{
    return static_cast<long long>(st.st_mtim.tv_sec) * 1000000000LL +
           static_cast<long long>(st.st_mtim.tv_nsec);
}

bool readWholeFileFromFd(int fd, std::size_t size, std::string& out)
{
    std::string buffer;
    buffer.resize(size);
    std::size_t done = 0;
    while (done < size)
    {
        const ssize_t n = pread(fd, &buffer[done], size - done, static_cast<off_t>(done));
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        if (n == 0)
        {
            return false;
        }
        done += static_cast<std::size_t>(n);
    }
    out.swap(buffer);
    return true;
}

enum GzipBuildResult
{
    GZIP_BUILT,
    GZIP_NOT_SMALLER,
    GZIP_UNAVAILABLE
};

/* returns a cached or freshly built gzip body for path+size+stamp. */
GzipBuildResult gzipBody(int fd, const std::string& path, long long size, long long stamp,
                         std::string& out)
{
    if (GzipCache::instance().get(path, size, stamp, out))
    {
        return GZIP_BUILT;
    }
    std::string raw;
    if (!readWholeFileFromFd(fd, static_cast<std::size_t>(size), raw))
    {
        return GZIP_UNAVAILABLE;
    }
    std::string compressed;
    if (!gzipCompress(raw.data(), raw.size(), compressed))
    {
        return GZIP_UNAVAILABLE;
    }
    if (compressed.size() >= raw.size())
    {
        return GZIP_NOT_SMALLER;
    }
    GzipCache::instance().put(path, size, stamp, compressed);
    out.swap(compressed);
    return GZIP_BUILT;
}

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
    if (strcasecmp(dot, ".webmanifest") == 0)
    {
        return "application/manifest+json";
    }
    if (strcasecmp(dot, ".eot") == 0)
    {
        return "application/vnd.ms-fontobject";
    }
    if (strcasecmp(dot, ".otf") == 0)
    {
        return "font/otf";
    }
    if (strcasecmp(dot, ".wasm") == 0)
    {
        return "application/wasm";
    }
    return "application/octet-stream";
}
}  // namespace

http_handler_simple::http_handler_simple()
    : m_not_found(false),
      m_file_address(nullptr),
      m_fd(-1),
      m_useGzip(false),
      m_stamp(0)
{
    m_real_file[0] = '\0';
    memset(&m_file_stat, 0, sizeof(m_file_stat));
}

http_handler_simple::~http_handler_simple()
{
    if (m_fd >= 0)
    {
        close(m_fd);
        m_fd = -1;
    }
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
    m_fd = open(m_real_file, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (m_fd < 0)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NO_RESOURCE);
        return false;
    }

    if (fstat(m_fd, &m_file_stat) < 0)
    {
        close(m_fd);
        m_fd = -1;
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NO_RESOURCE);
        return false;
    }

    if (!S_ISREG(m_file_stat.st_mode) && !S_ISDIR(m_file_stat.st_mode))
    {
        close(m_fd);
        m_fd = -1;
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NO_RESOURCE);
        return false;
    }

    if ( ! ( m_file_stat.st_mode & S_IROTH ) )
    {
        close(m_fd);
        m_fd = -1;
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::FORBIDDEN_REQUEST);
        return false;
    }

    if ( S_ISDIR( m_file_stat.st_mode ) )
    {
        close(m_fd);
        m_fd = -1;
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

    m_stamp = fileStamp(m_file_stat);
    const bool canCompress = globalAsset.gzip_enabled &&
                             isCompressibleType(contentTypeForPath(m_real_file)) &&
                             m_file_stat.st_size > 0 &&
                             m_file_stat.st_size >= globalAsset.gzip_min_size &&
                             static_cast<off_t>(kGzipMaxInput) >= m_file_stat.st_size;
    if (canCompress)
    {
        resp.setHeader("Vary", "Accept-Encoding");
    }

    const std::string etag = ETagCache::instance().etagFor(m_real_file, m_file_stat);
    const bool notBeneficial = canCompress &&
        GzipCache::instance().isCompressionNotBeneficial(m_real_file, m_file_stat.st_size, m_stamp);
    std::string variantEtag = etag;
    if (canCompress && !notBeneficial &&
        acceptsGzipEncoding(req.getHeader("Accept-Encoding")))
    {
        const std::string gzipEtag = gzipVariant(etag);
        if (!gzipEtag.empty())
        {
            variantEtag = gzipEtag;
            m_useGzip = true;
        }
    }
    if (!variantEtag.empty())
    {
        resp.setHeader("ETag", variantEtag);
    }
    const std::string lastModified = httpDate(m_file_stat.st_mtime);
    if (!lastModified.empty())
    {
        resp.setHeader("Last-Modified", lastModified);
    }
    const bool isHtml = strcmp(contentTypeForPath(m_real_file), "text/html; charset=utf-8") == 0;
    resp.setHeader("Cache-Control", isHtml ? "no-cache" : "public, max-age=600");

    bool notModified = false;
    if (!m_not_found)
    {
        const std::string ifNoneMatch = req.getHeader("If-None-Match");
        if (!ifNoneMatch.empty())
        {
            notModified = etagMatches(ifNoneMatch, variantEtag);
        }
        else
        {
            const std::string ifModifiedSince = req.getHeader("If-Modified-Since");
            std::time_t since = 0;
            if (!ifModifiedSince.empty() && parseHttpDate(ifModifiedSince, since) &&
                m_file_stat.st_mtime <= since)
            {
                notModified = true;
            }
        }
    }
    if (notModified)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::NOT_MODIFIED);
        return false;
    }

    /* build the compressed representation once so HEAD and GET agree, and
       fall back to identity (including its ETag) when compression is not
       possible or not smaller. Remember the not-smaller verdict so later
       revalidations take the identity path and can still answer 304. */
    if (m_useGzip)
    {
        const GzipBuildResult result =
            gzipBody(m_fd, m_real_file, m_file_stat.st_size, m_stamp, m_gzipBody);
        if (result == GZIP_NOT_SMALLER)
        {
            GzipCache::instance().markCompressionNotBeneficial(
                m_real_file, m_file_stat.st_size, m_stamp);
        }
        if (result != GZIP_BUILT)
        {
            m_useGzip = false;
            resp.setHeader("ETag", etag);
        }
    }

    if (req.getMethod() == "HEAD")
    {
        std::size_t length = static_cast<std::size_t>(m_file_stat.st_size);
        if (m_useGzip)
        {
            length = m_gzipBody.size();
            resp.setHeader("Content-Encoding", "gzip");
        }
        resp.setHeader("Content-Length", std::to_string(length));
        if (!m_not_found)
        {
            resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::GOOD_REQUEST);
        }
        return false;
    }
    return true;
}

bool http_handler_simple::handler(HttpRequest& req, HttpResponse& resp)
{
    if (m_useGzip)
    {
        resp.setHeader("Content-Encoding", "gzip");
        resp.setBody(m_gzipBody);
        if (!m_not_found)
        {
            resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::GOOD_REQUEST);
        }
        return true;
    }

    if (m_fd < 0)
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
        close(m_fd);
        m_fd = -1;
        return true;
    }

    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, m_fd, 0 );
    close(m_fd);
    m_fd = -1;
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