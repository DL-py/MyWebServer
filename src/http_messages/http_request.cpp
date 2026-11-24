#include "http_request.h"

#include <cctype>

namespace
{
/* HTTP field names are case-insensitive (RFC 9110 5.1). */
std::string normalizeHeaderName(const std::string& name)
{
    std::string normalized = name;
    for (std::size_t i = 0; i < normalized.size(); ++i)
    {
        normalized[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(normalized[i])));
    }
    return normalized;
}
}  // namespace

/*request line part*/
std::string HttpRequest::getMethod()
{
    return method_;
}

void HttpRequest::setMethod(std::string method)
{
    method_ = method;
}

std::string& HttpRequest::getURL()
{
    return url_;
}

void HttpRequest::setURL(std::string url)
{
    url_ = url;
}

std::string HttpRequest::getVersion()
{
    return version_;
}

void HttpRequest::setVersion(std::string version)
{
    version_ = version;
}

/*header part*/
const std::string HttpRequest::getHeader(std::string name)
{
    std::string value;

    auto it = headers_.find(normalizeHeaderName(name));
    if (it != headers_.end())
    {
        value =  it->second;
    }
    else
    {
        value = "";
    }

    return value;
}

void HttpRequest::setHeader(std::string name, std::string value)
{
    const std::string key = normalizeHeaderName(name);
    auto it = headers_.find(key);
    if (it == headers_.end())
    {
        headers_[key] = value;
        return;
    }
    /* RFC 9110 5.3: repeated list-valued fields combine as a comma list.
       Accept-Encoding is the one this server negotiates on. */
    if (key == "accept-encoding")
    {
        it->second += ", ";
        it->second += value;
    }
}

/*content part*/
std::string HttpRequest::getBody()
{
    return body_;
}

void HttpRequest::setBody(std::string body)
{
    body_ = body;
}