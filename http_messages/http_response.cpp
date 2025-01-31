#include "http_response.h"

/*status line part*/
int HttpResponse::getStatusCode()
{
    return statusCode_;
}

void HttpResponse::setStatusCode(int statusCode)
{
    statusCode_ = statusCode;
}

std::string HttpResponse::getStatusMessage()
{
    return statusMessage_;
}

void HttpResponse::setStatusMessage(std::string statusMessage)
{
    statusMessage_ = statusMessage;
}

void HttpResponse::setStatusCodeAndMessage(int statusCode)
{
    statusCode_ = statusCode;
    auto it = HttpResponseCode::httpResponseCodeInfo.find((HttpResponseCode::HTTP_RESPONSE_CODE)statusCode_);
    assert(it != HttpResponseCode::httpResponseCodeInfo.end());
    statusMessage_ = it->second;
}

void HttpResponse::setStatusCodeAndMessage(int statusCode, std::string statusMessage)
{
    statusCode_ = statusCode;
    statusMessage_ = statusMessage;
}

/*header part*/
const std::string HttpResponse::getHeader(std::string name)
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

void HttpResponse::setHeader(std::string name, std::string value)
{
    auto it = headers_.find(name);
    if (it == headers_.end())
    {
        headers_[name] = value;
    }
}

const std::unordered_map<std::string, std::string>& HttpResponse::getHeaders()
{
    return headers_;
}

/*content part*/
std::string& HttpResponse::getBody()
{
    return body_;
}

void HttpResponse::setBody(std::string body)
{
    body_ = body;
}