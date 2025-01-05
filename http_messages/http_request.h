#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#include <string>
#include <unordered_map>
#include "http_request_method.h"

class HttpRequest
{
private:
    /*requst line part*/
    std::string method_;
    std::string url_;
    std::string version_;

    /*header part*/
    std::unordered_map<std::string, std::string> headers_;

    /*content part*/
    std::string body_;

public:
    /*request line part*/
    std::string getMethod();
    void setMethod(std::string method);
    std::string getURL();
    void setURL(std::string url);
    std::string getVersion();
    void setVersion(std::string version);

    /*header part*/
    std::string getHeader(std::string name);
    void setHeader(std::string name, std::string value);

    /*content part*/
    std::string getBody();
    void setBody(std::string body);

};


#endif
