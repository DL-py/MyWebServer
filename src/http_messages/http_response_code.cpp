#include "http_response_code.h"

const std::unordered_map<HttpResponseCode::HTTP_RESPONSE_CODE, std::string> HttpResponseCode::httpResponseCodeInfo = 
{
    {INIT_RESPONSE_CODE, "Init Response Code"},
    {GOOD_REQUEST, "OK"},
    {MOVED_PERMANENTLY, "Moved Permanently"},
    {NOT_MODIFIED, "Not Modified"},
    {MOVED_TEMPORARILY, "Found"},
    {BAD_REQUEST, "Bad Request"},
    {UNAUTHORIZED, "Unauthorized"},
    {NO_RESOURCE, "Not Found"},
    {FORBIDDEN_REQUEST,"Forbidden"},
    {METHOD_NOT_ALLOWED, "Method Not Allowed"},
    {CONFLICT, "Conflict"},
    {INTERNAL_ERROR, "Internal Error"},
    {SERVICE_UNAVAILABLE, "Service Unavailable"},
};