#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/types.h>

#include "locker.h"
#include "threadpool.h"
#include "./global/global.hpp"
#include "./http_utils/http_conn.h"
#include "./handler/handler.h"
#include "./config/config.hpp"
#include "./logs/run_log.hpp"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern int addfd( int epollfd, int fd, bool one_shot );
extern int removefd( int epollfd, int fd );

void addsig( int sig, void( handler )(int), bool restart = true )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    if( restart )
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

void show_error( int connfd, const char* info )
{
    LogRecord(logger, LogLevel::LOG_ERR, "%s\n", info);
    send( connfd, info, strlen( info ), 0 );
    close( connfd );
}


int main( int argc, char* argv[] )
{
    /* initialize configuration. */
    globalCFG.initConfig("./config/common.cfg");
    const Config& cfg = globalCFG.getConfig();

    /* create and initialize logger. */
    if (!logger.initLogger())
    {
        std::cerr << "logger init error." << std::endl;
        return -1;
    }
    /* print logger information. */
    logger.printLogger();

    /* create and initialize asset. */
    if (!globalAsset.initAsset())
    {
        std::cerr << "asset init error." << std::endl;
        return -1;
    }
    globalAsset.PrintAsset();

    pthread_t tid = pthread_self();
    if (pthread_setname_np(tid, "leader") != 0)
    {
        std::cerr << "set thread name: " << "leader" <<" failed." << std::endl;
    }

    const char* ip = nullptr;
    int port;
    try
    {
        ip = cfg.lookup("conn.ip");
        port = cfg.lookup("conn.port");
    } 
    catch (const SettingNotFoundException& nfex)
    {
        LogRecord(logger, LogLevel::LOG_ERR, "%s is not found in configuration. \n", nfex.getPath());
        return -1;
    }

    addsig( SIGPIPE, SIG_IGN );

    threadpool<HttpConn>* pool = NULL;
    try
    {
        pool  = new threadpool<HttpConn>;
    }
    catch( ... )
    {
        return -1;
    }

    HttpConn* users = new HttpConn[ MAX_FD ];

    assert( users );
    int user_count = 0;

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );
    struct linger tmp = { 1, 0 };
    setsockopt( listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof( tmp ) );

    /* bind the socket fd to socket address {IP, port}. */
    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret >= 0 );

    ret = listen( listenfd, 5 );
    assert( ret >= 0 );

    epoll_event events[ MAX_EVENT_NUMBER ];
    int epollfd = epoll_create( 5 );
    assert( epollfd != -1 );
    addfd( epollfd, listenfd, false );

    HttpConn::epollfd = epollfd;

    while( true )
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            LogRecord(logger, LogLevel::LOG_EMERG, "epoll failed.\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                LogRecord(logger, LogLevel::LOG_INFO, "New connection....  ip: %s , port: %d\n", 
                                  inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                if (connfd < 0)
                {
                    LogRecord(logger, LogLevel::LOG_ERR, "New connection error, errno is: %d \n", errno);
                    continue;
                }
                if(HttpConn::userCount >= MAX_FD){
                    show_error(connfd, "Internal server busy");
                    continue;
                }
                users[connfd].initConn(connfd, client_address);
            }
            else if(events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ))
            {
                LogRecord(logger, LogLevel::LOG_INFO, "close socket: client close socket active(epoll).\n");
                users[sockfd].closeConn();
            }
            else if(events[i].events & EPOLLIN)
            {
                if (users[sockfd].bufferRead())
                {
                    pool->append(users + sockfd);
                }
                else
                {
                    users[sockfd].closeConn();
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                if (! users[sockfd].bufferWrite()) 
                {
                    users[sockfd].closeConn();
                }
            }
            else
            {}
        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;
    return 0;
}
