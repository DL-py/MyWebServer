#include <router.h>

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

    // if (method == "GET")
    // {
    //     
    // }
    // else if (method == "POST")
    // {

    // }
    // else
    // {

    // }
}


http_handler* http_handler_produce(http_com::HTTP_CODE http_code, http_com::METHOD method, char* url, char* content, int content_len)
{
    http_handler* handler = nullptr;
    if (method == http_com::GET)
    {
        
    }
    else if(method == http_com::POST)
    {
        handler = new http_handler_cgi(url, content, content_len);
    }
    else
    {
        return handler;
    }
    return handler;
}