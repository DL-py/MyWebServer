#include "http_request_method.h"

const std::unordered_map<std::string, HttpRequestMethod::HTTP_REQUEST_METHOD> HttpRequestMethod::httpRequestMethods_ = 
{
    {"GET", HTTP_REQUEST_METHOD::GET},
    {"POST", HTTP_REQUEST_METHOD::POST},
    {"HEAD", HTTP_REQUEST_METHOD::HEAD},
    {"PUT", HTTP_REQUEST_METHOD::PUT},
    {"DELETE", HTTP_REQUEST_METHOD::DELETE},
    {"TRACE", HTTP_REQUEST_METHOD::TRACE},
    {"OPTIONS", HTTP_REQUEST_METHOD::OPTIONS},
    {"CONNECT", HTTP_REQUEST_METHOD::CONNECT},
    {"PATCH", HTTP_REQUEST_METHOD::PATCH}
};


bool HttpRequestMethod::HttpRequestMethodCheck(std::string method)
{
    auto it = httpRequestMethods_.find(method);
    if (it != httpRequestMethods_.end())
    {
        return true;
    }
    else
    {
        return false;
    }
}