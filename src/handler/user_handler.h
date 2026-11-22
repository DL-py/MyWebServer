#ifndef MYWEBSERVER_HTTP_HANDLER_USER_H
#define MYWEBSERVER_HTTP_HANDLER_USER_H
#include "handler.h"

class http_user_handler : public http_handler {
public:
    http_user_handler();
    ~http_user_handler();
    bool handler_pre(HttpRequest& req, HttpResponse& resp) override;
    bool handler(HttpRequest& req, HttpResponse& resp) override;
    bool handler_post(HttpRequest& req, HttpResponse& resp) override;

private:
    bool handleRegister(HttpRequest& req, HttpResponse& resp);
    bool handleLogin(HttpRequest& req, HttpResponse& resp);
    bool handleLogout(HttpRequest& req, HttpResponse& resp);
    bool handleMe(HttpRequest& req, HttpResponse& resp);
};
#endif
