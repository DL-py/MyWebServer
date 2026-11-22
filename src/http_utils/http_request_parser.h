#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H
#include <cassert>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include "../http_messages/http_request.h"
#include "../http_messages/http_response.h"
#include "../http_messages/http_response_code.h"
#include "../http_messages/http_request_method.h"

class HttpRequestParser
{
private:
    enum ParserState {PARSER_STATE_REQUESTLINE = 0,  PARSER_STATE_HEADER, PARSER_STATE_CONTENT};

private:
    const size_t httpRequestLineFieldSize = 3;

private:
    ParserState parserState_;
    std::string parserBuffer_;
    std::size_t parserPos_;
    std::string parserLine_;

public:
    HttpRequestParser(std::string parserBuffer);
    bool httpRequestParser(HttpRequest& req, HttpResponse& resp);

private:
    bool getOneLine(HttpRequest& req, HttpResponse& resp);

    bool parseRequestLine(HttpRequest& req, HttpResponse& resp);
    bool parseHeaders(HttpRequest& req, HttpResponse& resp);
    bool parseContent(HttpRequest& req, HttpResponse& resp);

private:
    bool urlDecode(const std::string& encode, std::string& decode);
};



#endif