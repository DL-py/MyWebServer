#include "http_conn.h"

int HttpConn::userCount = 0;
int HttpConn::epollfd = -1;

// utility
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if( one_shot )
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    shutdown(fd, SHUT_WR);
    close(fd);
}

//interface
void HttpConn::initConn(int fd, const sockaddr_in& addr)
{
    sockfd = fd;
    address = addr;
    linger = false;
    writePos_ = 0;

    int error = 0;
    socklen_t len = sizeof( error );
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    addfd(epollfd, sockfd, true );
    userCount++;
}

void HttpConn::resetConn()
{
    modfd(epollfd, sockfd, EPOLLIN);
    linger = false;
    writePos_ = 0;
    readBuffer.clear();
    writeBuffer.clear();
}

void HttpConn::closeConn()
{
    if(sockfd != -1)
    {
        removefd(epollfd, sockfd);
        sockfd = -1;
        userCount--;
    }
}

bool HttpConn::bufferRead()
{
    if (readBuffer.size() >= HTTP_REQUEST_SIZE)
    {
        LogRecord(logger, LogLevel::LOG_ALERT, "close socket: readBuffer overflow.\n");
        return false;
    }

    char buffer[READ_BUFFER_SIZE];
    while(readBuffer.size() + READ_BUFFER_SIZE < HTTP_REQUEST_SIZE)
    {
        int bytesRead = recv( sockfd, buffer, READ_BUFFER_SIZE, 0 );

        if ( bytesRead == -1 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                break;
            }
            LogRecord(logger, LogLevel::LOG_ERR, "close socket: read data from socket error.\n");
            return false;
        }
        else if ( bytesRead == 0 )
        {
            LogRecord(logger, LogLevel::LOG_INFO, "close socket: client close socket active(read).\n");
            return false;
        }

        readBuffer.append(buffer, bytesRead);
    }

    return true;
}

bool HttpConn::bufferWrite()
{
    int writePos = writePos_;
    int bytesToWrite = writeBuffer.size() - writePos;
    LogRecord(logger, LogLevel::LOG_DEBUG, "response has %d bytes to be written to socket. \n",  bytesToWrite);

    while (1)
    {
        int sendSize = bytesToWrite >= WRITE_BUFFER_SIZE ? WRITE_BUFFER_SIZE : bytesToWrite;
        int ret = send(sockfd, writeBuffer.substr(writePos, sendSize).c_str(), sendSize, 0);

        LogRecord(logger, LogLevel::LOG_DEBUG, "response has %d bytes has been written to socket. \n", ret);
        // send error
        if (ret < 0)  
        {
            if (errno == EAGAIN)
            {
                modfd(epollfd, sockfd, EPOLLOUT);  // write again
                LogRecord(logger, LogLevel::LOG_DEBUG, "socket isn't ready to write, now ready to try again. \n");
                writePos_ = writePos;
                return true;
            }

            LogRecord(logger, LogLevel::LOG_ERR, "close socket: write data to socket error.\n");
            return false;  // close connection
        }

        // send succeed
        bytesToWrite -=  ret;
        writePos += ret;

        LogRecord(logger, LogLevel::LOG_DEBUG, "response write pos is %d \n", writePos);
        // send finish
        if (bytesToWrite <= 0)
        {
            resetConn();
            return true;
        }
    }

}

void HttpConn::process()
{
    HttpRequest httpRequest;
    HttpResponse httpResponse;
    HttpRequestParser httpRequestParser(readBuffer);

    httpRequestParser.httpRequestParser(httpRequest, httpResponse);

    http_handler* handler = Router::route(httpRequest, httpResponse);

    if (! handler->handler_pre(httpRequest, httpResponse))
    {
        // write response to socket.
        goto response;
    }

    if (! handler->handler(httpRequest, httpResponse))
    {
        // write response to socket.
        goto response;
    }

    if (! handler->handler_post(httpRequest, httpResponse))
    {
        goto response;
    }

    Router::handlerDestory(handler);

response:
    HttpResponseParser httpResponseParser;
    httpResponseParser.httpResponseParser(httpRequest, httpResponse);

    const std::string& parseBuffer = httpResponseParser.getParserBuffer();

    writeBuffer = std::move(parseBuffer);

    const std::string conn = httpResponse.getHeader("Connection");
    if (conn == "keep-alive")
    {
        linger = true;
    }

    modfd(epollfd, sockfd, EPOLLOUT);
}

void HttpConn::setWritebuffer(std::string& buffer)
{
    writeBuffer = std::move(buffer);
}

const std::string& HttpConn::getWriteBuffer()
{
    return writeBuffer;
}