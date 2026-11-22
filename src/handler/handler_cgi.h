#ifndef MYWEBSERVER_HTTP_HANDLER_CGI_H
#define MYWEBSERVER_HTTP_HANDLER_CGI_H

#include <string>
#include "handler.h"

class http_handler_cgi : public http_handler
{
public:
    http_handler_cgi();
    virtual ~http_handler_cgi();

    bool handler_pre(HttpRequest& req, HttpResponse& resp) override;
    bool handler(HttpRequest& req, HttpResponse& resp) override;
    bool handler_post(HttpRequest& req, HttpResponse& resp) override;

private:
    std::string m_scriptPath;
    std::string m_docRoot;

    bool buildScriptPath(HttpRequest& req, HttpResponse& resp);
    bool execute(HttpRequest& req, HttpResponse& resp, std::string& output);
    void parseCgiOutput(const std::string& output, HttpResponse& resp);
};

#endif
