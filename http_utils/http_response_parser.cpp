#include "http_response_parser.h"

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

const char* HttpResponseParser::getContentType(const std::string& url)
{
    int length = url.size();
#define SUFFIXLENGTH  6
    if (length > SUFFIXLENGTH)
    {
        const std::string suffix = url.substr(length - SUFFIXLENGTH, SUFFIXLENGTH);
        if (suffix == ".woff2")
        {
            return "font/woff2";
        }
    }
#undef SUFFIXLENGTH

#define SUFFIXLENGTH  5
    if (length > SUFFIXLENGTH)
    {
        const std::string suffix = url.substr(length - SUFFIXLENGTH, SUFFIXLENGTH);
        if (suffix == ".jpeg")
        {
            return "image/jpeg";
        }
        else if (suffix == ".webp")
        {
            return "image/webp";
        }
    }
#undef SUFFIXLENGTH

#define SUFFIXLENGTH  4
    if (length > SUFFIXLENGTH)
    {
        const std::string suffix = url.substr(length - SUFFIXLENGTH, SUFFIXLENGTH);
        if (suffix == ".css")
        {
            return "text/css";
        }
        else if (suffix == ".jpg")
        {
            return "image/jpeg";
        }
    }
#undef SUFFIXLENGTH

#define SUFFIXLENGTH  3
    if (length > SUFFIXLENGTH)
    {
        const std::string suffix = url.substr(length - SUFFIXLENGTH, SUFFIXLENGTH);
        if (suffix == ".js")
        {
            return "text/javascript";
        }
    }
#undef SUFFIXLENGTH
    return nullptr;
}

bool HttpResponseParser::parseStatusLine(HttpRequest& req, HttpResponse& resp)
{
    using responseCode = HttpResponseCode::HTTP_RESPONSE_CODE;
    responseCode statusCode = (responseCode)resp.getStatusCode();
    const char* responseTitle = resp.getStatusMessage().c_str();

    add_response("%s %d %s\r\n", "HTTP/1.1", (int)statusCode, responseTitle);
}

bool HttpResponseParser::parseHeaders(HttpRequest& req, HttpResponse& resp)
{
    int bodySize = resp.getBody().size();
    resp.setHeader("Content-Length", std::to_string(bodySize));

    const std::string url = req.getURL();
    /*fontawesome.min.css*/
    const char* contentType = getContentType(url);
    if (contentType)
    {
        resp.setHeader("content-type", contentType);
    }

    const std::string conn = req.getHeader("Connection");
    if (conn == "keep-alive")
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

