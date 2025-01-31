#include "http_request.h"

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

    auto it = headers_.find(name);
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
    auto it = headers_.find(name);
    if (it == headers_.end())
    {
        headers_[name] = value;
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