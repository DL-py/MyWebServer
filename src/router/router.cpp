#include "router.h"

#include "../handler/handler_cgi.h"
#include "../handler/user_handler.h"

http_handler* Router::route(HttpRequest& req, HttpResponse& resp)
{
    using HttpMthod = HttpRequestMethod::HTTP_REQUEST_METHOD;

    if (! HttpRequestMethod::HttpRequestMethodCheck(req.getMethod()))
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return nullptr;
    }

    HttpRequestMethod::HTTP_REQUEST_METHOD method = HttpRequestMethod::HttpRequestMethodGet(req.getMethod());

    std::string url = req.getURL();
    size_t queryPos = url.find('?');
    if (queryPos != std::string::npos)
    {
        url.erase(queryPos);
    }
    const bool isCgi = url.size() >= 4 && url.compare(url.size() - 4, 4, ".cgi") == 0;

    if (url.rfind("/api/auth/", 0) == 0)
    {
        return new http_user_handler();
    }

    http_handler* handler = nullptr;
    switch (method)
    {
        case HttpMthod::GET:
        {
            if (isCgi)
            {
                handler = new http_handler_cgi();
            }
            else
            {
                handler = new http_handler_simple();
            }
            break;
        }
        case HttpMthod::POST:
        {
            if (isCgi)
            {
                handler = new http_handler_cgi();
            }
            else
            {
                resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
                return nullptr;
            }
            break;
        }
        default:
        {
            resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
            return nullptr;
        }
    }

    return handler;
}


void Router::handlerDestory(http_handler* handler)
{
    delete handler;
}