#include "../http_utils/http_request_parser.h"
#include "../http_messages/http_request.h"
#include "../http_messages/http_response.h"
#include <iostream>

static bool parse(const std::string& raw, HttpRequest& req, HttpResponse& resp)
{
    HttpRequestParser parser(raw);
    return parser.httpRequestParser(req, resp);
}

int main()
{
    HttpRequest emptyReq;
    HttpResponse emptyResp;
    const std::string empty =
        "POST /api/auth/logout HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n";
    if (!parse(empty, emptyReq, emptyResp) ||
        emptyResp.getStatusCode() == 400 ||
        emptyReq.getMethod() != "POST" || !emptyReq.getBody().empty())
    {
        std::cerr << "FAIL: zero-length body\n";
        return 1;
    }

    HttpRequest bodyReq;
    HttpResponse bodyResp;
    const std::string body =
        "POST /api/auth/login HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nhello";
    if (!parse(body, bodyReq, bodyResp) ||
        bodyResp.getStatusCode() == 400 || bodyReq.getBody() != "hello")
    {
        std::cerr << "FAIL: normal body\n";
        return 1;
    }

    std::cout << "http body test PASS\n";
    return 0;
}
