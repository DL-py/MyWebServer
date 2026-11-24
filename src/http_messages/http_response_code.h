#ifndef HTTP_RESPONSE_CODE_H
#define HTTP_RESPONSE_CODE_H
#include <string>
#include <unordered_map>

class HttpResponseCode
{
private:
    /*Forbid Instantiation*/
    HttpResponseCode() {};
    ~HttpResponseCode() {};
public:
    enum HTTP_RESPONSE_CODE 
    {
        INIT_RESPONSE_CODE = 000,
        GOOD_REQUEST = 200,
        MOVED_PERMANENTLY = 301,
        NOT_MODIFIED = 304,
        MOVED_TEMPORARILY = 302,
        BAD_REQUEST = 400,
        UNAUTHORIZED = 401,
        FORBIDDEN_REQUEST = 403,
        NO_RESOURCE = 404,
        METHOD_NOT_ALLOWED = 405,
        CONFLICT = 409,
        INTERNAL_ERROR = 500,
        SERVICE_UNAVAILABLE = 503,
    };

    const static std::unordered_map<HTTP_RESPONSE_CODE, std::string> httpResponseCodeInfo;
    
};


#endif