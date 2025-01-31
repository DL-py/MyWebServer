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
        BAD_REQUEST = 400,
        FORBIDDEN_REQUEST = 403,
        NO_RESOURCE = 404,
        INTERNAL_ERROR = 500,
    };

    const static std::unordered_map<HTTP_RESPONSE_CODE, std::string> httpResponseCodeInfo;
    
};


#endif