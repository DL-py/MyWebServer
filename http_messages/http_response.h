#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#include <cassert>
#include <string>
#include <unordered_map>

#include "http_response_code.h"
class HttpResponse
{
private:
    /*status line part*/
    int statusCode_;
    std::string statusMessage_;
    
    /*header part*/
    std::unordered_map<std::string, std::string> headers_;
    
    /*content part*/
    std::string body_;

public:
    /*status line part*/
    int getStatusCode();
    void setStatusCode(int statusCode);
    std::string getStatusMessage();
    void setStatusMessage(std::string statusMessage);
    void setStatusCodeAndMessage(int statusCode);
    void setStatusCodeAndMessage(int statusCode, std::string statusMessage);

    /*header part*/
    const std::string getHeader(std::string name);
    const std::unordered_map<std::string, std::string>& getHeaders();
    void setHeader(std::string name, std::string value);

    /*content part*/
    std::string& getBody();
    void setBody(std::string body);

};
#endif