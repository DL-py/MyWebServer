#include "router.h"

http_handler* Router::route(HttpRequest& req, HttpResponse& resp)
{
    using HttpMthod = HttpRequestMethod::HTTP_REQUEST_METHOD;

    if (! HttpRequestMethod::HttpRequestMethodCheck(req.getMethod()))
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return nullptr;
    }

    HttpRequestMethod::HTTP_REQUEST_METHOD method = HttpRequestMethod::HttpRequestMethodGet(req.getMethod());

    http_handler* handler = nullptr;
    switch (method)
    {
        case HttpMthod::GET:
        {
            handler = new http_handler_simple();
            break;
        }
        case HttpMthod::POST:
        {
            break;
        }
        default:
        {
            resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
            return nullptr;
        }
    }
}


void Router::handlerDestory(http_handler* handler)
{
    delete handler;
}