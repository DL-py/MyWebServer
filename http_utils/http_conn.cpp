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
    close(fd);
}

//interface
void HttpConn::initConn(int fd, const sockaddr_in& addr)
{
    sockfd = fd;
    address = addr;
    linger = false;

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
    readBuffer.clear();
    writeBuffer.clear();
}

void HttpConn::closeConn()
{
    if(sockfd != -1)
    {
        removefd(epollfd, sockfd );
        sockfd = -1;
        userCount--;
    }
}

bool HttpConn::bufferRead()
{
    if (readBuffer.size() >= HTTP_REQUEST_SIZE)
    {
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
            return false;
        }
        else if ( bytesRead == 0 )
        {
            return false;
        }

        readBuffer.append(buffer, bytesRead);
    }

    return true;
}

bool HttpConn::bufferWrite()
{
    int bytesToWrite = writeBuffer.size();
    int writePos = 0;

    while (1)
    {
        int sendSize = bytesToWrite >= WRITE_BUFFER_SIZE ? WRITE_BUFFER_SIZE : bytesToWrite;
        int ret = send(sockfd, writeBuffer.substr(writePos, sendSize).c_str(), sendSize, 0);

        // send error
        if (ret < 0)  
        {
            if (errno == EAGAIN)
            {
                modfd(epollfd, sockfd, EPOLLOUT);  // write again
                return true;
            }

            return false;  // close connection
        }

        // send succeed
        bytesToWrite -=  sendSize;
        writePos += sendSize;

        // send finish
        if (bytesToWrite <= 0)
        {
            // keep alive, reset for next http request
            if(linger)
            {
                resetConn();
                return true;
            }
            else
            {
                // close connection
                return false;
            }
        }
    }

}

void HttpConn::process()
{
    HttpRequest httpRequest;
    HttpResponse httpResponse;
    HttpRequestParser httpRequestParser(readBuffer);

    httpRequestParser.httpRequestParser(httpRequest, httpResponse);

}
