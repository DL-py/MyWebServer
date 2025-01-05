#include "http_response_code.h"

const std::unordered_map<HttpResponseCode::HTTP_RESPONSE_CODE, std::string> HttpResponseCode::httpResponseCodeInfo = 
{
    {INIT_RESPONSE_CODE, "Init Response Code"},
    {BAD_REQUEST, "Bad Request"},
};