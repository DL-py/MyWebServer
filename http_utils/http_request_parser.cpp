#include "http_request_parser.h"


HttpRequestParser::HttpRequestParser(std::string parserBuffer):parserState_(PARSER_STATE_REQUESTLINE), parserBuffer_(parserBuffer), parserPos_(0)
{

}

bool HttpRequestParser::httpRequestParser(HttpRequest& req, HttpResponse& resp)
{
    while(getOneLine(req, resp))
    {
        switch(parserState_)
        {
            case PARSER_STATE_REQUESTLINE:
            {
                if (!parseRequestLine(req, resp))
                {
                    return false;
                }
                parserState_ = PARSER_STATE_HEADER;
            }
            break;
            case PARSER_STATE_HEADER:
            {
                if (parserLine_.empty())  /*HTTP request header ends*/
                {
                    std::string contentLengthString = req.getHeader("Content-Length");
                    
                    if (contentLengthString.empty())
                    {
                        return true;
                    }
                    else
                    {
                        parserState_ = PARSER_STATE_CONTENT;
                    }
                }
                else
                {
                    if (!parseHeaders(req, resp))
                    {
                        return false;
                    }
                }
            }
            break;
            case PARSER_STATE_CONTENT:
            {
                if (!parseContent(req, resp))
                {
                    return false;
                }
            }
            break;
            default:
            {
                assert(0);
            }   
        }
    }

    return true;
}

bool HttpRequestParser::getOneLine(HttpRequest& req, HttpResponse& resp)
{
    switch (parserState_)
    {
        case PARSER_STATE_REQUESTLINE:
        case PARSER_STATE_HEADER:
        {
            std::size_t foundPos = parserBuffer_.find("\r\n", parserPos_);
            /**
             *     POST / HTTP/1.1\r\nHost:www.baidu.com\r\n
             *     ^              ^
             *     |              |
             * parserPos_      foundPos  
            */
            if (foundPos != std::string::npos)
            {
                std::size_t length = foundPos - parserPos_;
                parserLine_ = parserBuffer_.substr(parserPos_, length);
                parserPos_ = foundPos + 2;  // skip \r\n
                return true;
            }
            else
            {
                resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
                return false;
            }
        }
        case PARSER_STATE_CONTENT:
        {
            if (parserPos_ >= parserBuffer_.size())
            {
                resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
                return false;
            }

            std::size_t realContentLength = parserBuffer_.size() - parserPos_;
            std::string theoryContentLengthString = req.getHeader("Content-Length");
            std::size_t theoryContentLength = atoi(theoryContentLengthString.c_str());

            if (realContentLength != theoryContentLength)
            {
                resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
                return false;                
            }

            parserLine_ = parserBuffer_.substr(parserPos_, theoryContentLength);
            parserPos_ = parserPos_ + theoryContentLength;

            return true;
        }
        default:
        {
            assert(0);
        }
    }
}

bool HttpRequestParser::parseRequestLine(HttpRequest& req, HttpResponse& resp)
{
    if (parserLine_.empty())
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return false;
    }

    std::vector<std::string> reqestLineFields;
    const char delimiter = ' ';

    /**
     * "GET /welcome.html HTTP/1.1"
     *  ^  ^                     ^
     *  |  |                     |
     *  | foundPos               |
     * startPos               endpos
    */
    size_t startPos = 0;
    size_t foundPos = parserLine_.find(delimiter, startPos);
    size_t endPos = startPos + (parserLine_.size()-1);
    while(foundPos != parserLine_.npos || startPos <= endPos)
    {
        size_t len;
        if (foundPos != parserLine_.npos)
        {
            len = foundPos - startPos;
        }
        else
        {
            len = endPos - startPos + 1;
        }

        std::string field = parserLine_.substr(startPos, len);
        reqestLineFields.emplace_back(field);
        if (foundPos != parserLine_.npos)
        {
            startPos = foundPos + 1;
            foundPos = parserLine_.find(delimiter, startPos);
        }
        else 
        {
            startPos = endPos + 1;
        }

    }


    if (reqestLineFields.size() != httpRequestLineFieldSize)
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return false;
    }

    std::string method = reqestLineFields[0];
    if (!HttpRequestMethod::HttpRequestMethodCheck(method))
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return false;
    }
    else
    {
        req.setMethod(method);
    }

    std::string url = reqestLineFields[1];
    if (url.find("http://") == 0)
    {
        url.erase(0, strlen("http://"));
    }
    
    if (!url.empty() && url[0] == '/')
    {
        std::string& url_ = req.getURL();
        urlDecode(url, url_);
    }
    else
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return false;
    }

    std::string version = reqestLineFields[2];
    if (version == "HTTP/1.1")
    {
        req.setVersion(version);
    }
    else
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return false;        
    }

    return true;
}

bool HttpRequestParser::parseHeaders(HttpRequest& req, HttpResponse& resp)
{
    std::size_t startPos = 0;
    std::size_t foundPos = parserLine_.find(':', startPos);
    /**
     *     Host:www.baidu.com
     *     ^   ^
     *     |   |
     *     0 foundPos  
    */
    if (foundPos != std::string::npos)
    {
        std::size_t name_length = foundPos - startPos;
        std::string name = parserLine_.substr(startPos, name_length);
        
        startPos = foundPos + 1;
        while(startPos < parserLine_.size() && parserLine_[startPos] == ' ')
        {
            ++startPos;
        }
        std::size_t value_length = parserLine_.size() - startPos;
        std::string value = parserLine_.substr(startPos, value_length);

        if (name.empty() || value.empty())
        {
            resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
            return false;
        }

        req.setHeader(name, value);

        return true;
    }
    else
    {
        resp.setStatusCodeAndMessage((int)HttpResponseCode::HTTP_RESPONSE_CODE::BAD_REQUEST);
        return false;
    }
}

bool HttpRequestParser::parseContent(HttpRequest& req, HttpResponse& resp)
{
    req.setBody(parserLine_);
    return true;
}

unsigned char fromHex(char ch)
{
    unsigned char val;
    if (isupper(ch))
    {
        val = ch - 'A' + 10;
    }
    else if (islower(ch))
    {
        val = ch - 'a' + 10;
    }
    else if (isdigit(ch))
    {
        val = ch - '0';
    }
    else
    {
        assert(0);
    }
    return val;
}

bool HttpRequestParser::urlDecode(const std::string& encode, std::string& decode)
{
    int size = encode.size();

    for(int i = 0; i < size; i++)
    {
        if (encode[i] == '+')
        {
            decode += ' ';
        }
        else if(encode[i] == '%')
        {
            if (i + 2 >= size)
            {
                return false;
            }
            unsigned char high = fromHex(encode[++i]);
            unsigned char low = fromHex(encode[++i]);

            decode += ((high << 4) + low);
        }
        else
        {
            decode += encode[i];
        }
    }
    return true;
}