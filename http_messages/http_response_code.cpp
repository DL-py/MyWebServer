#include "http_response_code.h"

const std::unordered_map<HttpResponseCode::HTTP_RESPONSE_CODE, std::string> HttpResponseCode::httpResponseCodeInfo = 
{
    {INIT_RESPONSE_CODE, "Init Response Code"},
    {GOOD_REQUEST, "OK"},
    {BAD_REQUEST, "Bad Request"},
    {NO_RESOURCE, "Not Found"},
    {FORBIDDEN_REQUEST,"Forbidden"},
    {INTERNAL_ERROR, "Internal Error"},
};