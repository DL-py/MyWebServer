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
        BAD_REQUEST = 400,
    };

    const static std::unordered_map<HTTP_RESPONSE_CODE, std::string> httpResponseCodeInfo;
    
};


#endif