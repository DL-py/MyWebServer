#ifndef HTTP_CONN_H
#define HTTP_CONN_H
#include <string>

// Linux Header
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "http_request_parser.h"
#include "http_response_parser.h"
#include "../router/router.h"
#include "../handler/handler.h"
#include "../http_messages/http_request.h"
#include "../http_messages/http_response.h"
#include "../http_messages/http_response_code.h"
#include "../http_messages/http_request_method.h"


class HttpConn
{
private:
    static const int HTTP_REQUEST_SIZE = 10 * 1024 * 1024;  // 10MB
    static const int READ_BUFFER_SIZE = 2 * 1024;  // 2KB

    static const int HTTP_RESPONSE_SIZE = 64 * 1024 * 1024; // 64MB
    static const int WRITE_BUFFER_SIZE = 8 * 1024; // 8KB

public:
	static int epollfd;
	static int userCount;

private:
    std::string  readBuffer;  // http request buffer
    std::string  writeBuffer; // http response buffer
    int sockfd;
	sockaddr_in address;
    bool linger;

public:
    HttpConn() {}
    ~HttpConn() {}

public:
    void initConn(int sockfd, const sockaddr_in& addr);
    void closeConn();
    bool bufferRead();
    bool bufferWrite();
    void process();

    void setWritebuffer(std::string& buffer);
    const std::string& getWriteBuffer();

private:
    void resetConn();
};
#endif