#ifndef HTTP_RESPONSE_PARSER_H
#define HTTP_RESPONSE_PARSER_H
#include <cassert>
#include <cstring>
#include <cstdarg>
#include <string>
#include <vector>
#include "../http_messages/http_request.h"
#include "../http_messages/http_response.h"
#include "../http_messages/http_response_code.h"
#include "../http_messages/http_request_method.h"

class HttpResponseParser
{
private:
    static const int BUFFER_SIZE = 8 * 1024; // 8KB
private:
    enum ParserState {PARSER_STATE_STATUSLINE = 0,  PARSER_STATE_HEADER, PARSER_STATE_CONTENT, PARSE_STATE_FINISH};
public:
    HttpResponseParser();
    ~HttpResponseParser();

    bool httpResponseParser(HttpRequest& req, HttpResponse& resp);

    const std::string&  getParserBuffer();

private:
    bool add_response(const char* format, ...);

    bool parseStatusLine(HttpRequest& req, HttpResponse& resp);
    bool parseHeaders(HttpRequest& req, HttpResponse& resp);
    bool parseContent(HttpRequest& req, HttpResponse& resp);

private:
    ParserState parserState_;
    std::string parserBuffer_;
    char buffer_[BUFFER_SIZE];
};

#endif