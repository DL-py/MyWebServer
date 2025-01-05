#ifndef HTTP_REQUEST_METHOD_H
#define HTTP_REQUEST_METHOD_H
#include <cassert>
#include <string>
#include <unordered_map>

class HttpRequestMethod
{
public:
    enum HTTP_REQUEST_METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH};
public:
    static const std::unordered_map<std::string, HTTP_REQUEST_METHOD> httpRequestMethods_;

    static bool HttpRequestMethodCheck(std::string method);

    static HTTP_REQUEST_METHOD HttpRequestMethodGet(const std::string& method);
};
#endif