#include "http_response_parser.h"

#include <ctime>

#include <strings.h>

#include "http_protocol.hpp"

namespace
{
bool hasHeaderIgnoreCase(HttpResponse& resp, const char* name)
{
    const std::unordered_map<std::string, std::string>& headers = resp.getHeaders();
    for (std::unordered_map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        if (strcasecmp(it->first.c_str(), name) == 0)
        {
            return true;
        }
    }
    return false;
}
}  // namespace

HttpResponseParser::HttpResponseParser():parserState_(ParserState::PARSER_STATE_STATUSLINE)
{

}

HttpResponseParser::~HttpResponseParser()
{
    
}

bool HttpResponseParser::add_response(const char* format, ...)
{
    va_list arg_list;
    va_start(arg_list, format);

    int len = vsnprintf(buffer_, BUFFER_SIZE, format, arg_list);
    parserBuffer_ += buffer_;

    va_end(arg_list);
    return true;
}

bool HttpResponseParser::parseStatusLine(HttpRequest& req, HttpResponse& resp)
{
    using responseCode = HttpResponseCode::HTTP_RESPONSE_CODE;
    responseCode statusCode = (responseCode)resp.getStatusCode();
    const std::string responseTitle = resp.getStatusMessage();

    add_response("%s %d %s\r\n", "HTTP/1.1", (int)statusCode, responseTitle.c_str());
    return true;
}

bool HttpResponseParser::parseHeaders(HttpRequest& req, HttpResponse& resp)
{
    int bodySize = resp.getBody().size();
    /* RFC 9110 8.6: a 304 must not carry a Content-Length unless it equals
       the length of the 200 body; omit it instead. */
    if (resp.getStatusCode() != 304 && resp.getHeader("Content-Length").empty())
    {
        resp.setHeader("Content-Length", std::to_string(bodySize));
    }

    if (!hasHeaderIgnoreCase(resp, "Date"))
    {
        const std::string now = httpDate(std::time(nullptr));
        if (!now.empty())
        {
            resp.setHeader("Date", now);
        }
    }

    /* keep the pre-existing opt-in semantics: only an explicit keep-alive
       token reuses the connection; close wins when both are present. */
    const std::string conn = req.getHeader("Connection");
    if (!headerHasToken(conn, "close") && headerHasToken(conn, "keep-alive"))
    {
        resp.setHeader("Connection", "keep-alive");
    }
    else
    {
        resp.setHeader("Connection", "close");
    }
    
    /* write headers to parserBuffer_. */
    const auto& headers = resp.getHeaders();
    for(const auto& pair : headers)
    {
        add_response("%s: %s\r\n", pair.first.c_str(), pair.second.c_str());
    }

    /*add final blank line. */
    add_response("%s", "\r\n");
    return true;
}

bool HttpResponseParser::parseContent(HttpRequest& req, HttpResponse& resp)
{
    std::string& body = resp.getBody();
    if (body.empty())
    {
        return true;
    }
    
    parserBuffer_ += body;

    return true;
}

bool HttpResponseParser::httpResponseParser(HttpRequest& req, HttpResponse& resp)
{
    while(parserState_ != PARSE_STATE_FINISH)
    {
        switch (parserState_)
        {
            case ParserState::PARSER_STATE_STATUSLINE:
            {
                parseStatusLine(req, resp);
                parserState_ = ParserState::PARSER_STATE_HEADER;
                break;
            }
            case ParserState::PARSER_STATE_HEADER:
            {
                parseHeaders(req, resp);
                parserState_ = ParserState::PARSER_STATE_CONTENT;
                break;
            }
            case ParserState::PARSER_STATE_CONTENT:
            {
                parseContent(req, resp);
                parserState_ = PARSE_STATE_FINISH;
                break;
            }
            default:
            {
                assert(0);
                break;
            }
        }
    }


    return true;
}

const std::string&  HttpResponseParser::getParserBuffer()
{
    return parserBuffer_;
}

