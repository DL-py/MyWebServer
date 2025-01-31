#ifndef HTTPCONN_H
#define HTTPCONN_H
// Linux Header File
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <mysql/mysql.h>
#include <iostream>
#include <vector>
#include <assert.h>
#include <locale>
#include <codecvt>
// User Header File
#include "locker.h"
#include "ConnPool.h"
#include "http_req.h"
#include "http_resp.h"
#include "http_com.h"
#include "http_handler.h"
#include "http_handler_simple.h"
#include "http_handler_factory.h"

extern "C"
{
#include "./logs/run_log.h"
}
using namespace std;

class http_conn
{
public:
	// static const int READ_BUFFER_SIZE = 2048;  /*读缓冲区的大小*/
    static const int READ_BUFFER_SIZE = 64;
	static const int WRITE_BUFFER_SIZE = 4096; /*写缓冲区的大小*/
public:
	http_conn() {}
	~http_conn() {}

public:
	void init(int sockfd, const sockaddr_in& addr);
    void reset();
	void close_conn(bool real_close = true);

public:
    bool buffer_read();
	bool buffer_write();

private:

    http_com::HTTP_CODE do_request();

public:
    void process();
    http_com::HTTP_CODE process_read();
    bool process_write( http_com::HTTP_CODE ret );

public:
	static int m_epollfd;
	static int m_user_count;

private:
    http_com m_http_com;
    http_req m_http_req;
    http_resp m_http_resp;
    http_handler* m_http_handler;

private:
	int m_sockfd;
	sockaddr_in m_address;

private:
    http_com::CHECK_STATE m_check_state;
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];

	struct iovec m_iv[2];
	int m_iv_count;
};

#endif
