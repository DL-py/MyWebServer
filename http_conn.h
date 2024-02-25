#ifndef HTTPCONN_H
#define HTTPCONN_H
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
#include<sys/uio.h>
#include <mysql/mysql.h>
#include <iostream>
#include <vector>
#include <assert.h>
#include <locale>
#include <codecvt>
#include "locker.h"
#include "ConnPool.h"
extern "C"
{
#include "user_handle.h"
#include "./logs/run_log.h"
#include "cgi_exec.h"
}
using namespace std;

class http_conn
{
public:
	static const int FILENAME_LEN = 200;  /*文件名的最大长度*/
	static const int READ_BUFFER_SIZE = 2048;  /*读缓冲区的大小*/
	static const int WRITE_BUFFER_SIZE = 4096; /*写缓冲区的大小*/

	enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH}; /*http请求方法，目前支持GET和POST*/
	enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT}; /*解析客户请求时，主机所处的状态*/
	enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNNECTION}; /*服务器处理HTTP请求的可能结果*/
	enum LINE_STATUS {LINE_OK, LINE_BAD, LINE_OPEN};
public:
	http_conn() {}
	~http_conn() {}

public:
	void init(int sockfd, const sockaddr_in& addr);  /*初始化接受的新连接*/
	void close_conn(bool real_close = true);   /*关闭连接*/
	void process();  /*处理客户请求*/
	bool my_read();   /*非阻塞读操作*/
	bool my_write();  /*非阻塞写操作*/

private:
	void init();  /*初始化连接*/
	HTTP_CODE process_read();   /*解析HTTP请求*/
	bool process_write(HTTP_CODE ret); /*填充HTTP应答*/
	
	/*下面这一组函数被process_read调用以分析HTTP请求*/
	LINE_STATUS parse_line();
	char * get_line() {return m_read_buf + m_start_line;}
	HTTP_CODE parse_request_line(char * text);
	HTTP_CODE parse_headers(char * text);
	HTTP_CODE parse_content(char * text);
	HTTP_CODE do_request();
	void execute_register_cgi();
	void execute_login_cgi();

	/*下面这一组函数被process_write调用以填充HTTP应答*/
	void unmap();
	bool add_response(const char* format, ...);
	bool add_content(const char * content);
	bool add_status_line(int status, const char *title);
	bool add_headers(int content_length);
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();

public:
	static int m_epollfd; /*所有socket都被注册到同一个epoll内核事件表，所以将epoll文件描述符设置为静态的*/
	static int m_user_count;  /*统计用户数量*/

private:
	int m_sockfd;  /* 该HTTP连接的socket */
	sockaddr_in m_address;  /* 客户端的socket地址 */

	char m_read_buf[READ_BUFFER_SIZE];   /*用户读缓冲区*/
	int m_read_idx;  /**just the length of http request*/
	int m_checked_idx;  /* index in m_read_buf, [0~m_checked_idx) in m_read_buf has been parsed*/
	int m_start_line;  /*正在解析的行的起始位置*/
	char m_write_buf[WRITE_BUFFER_SIZE]; /*用户写缓冲区*/
	int m_write_idx;  /*写缓冲区中待发送的字节数*/

	CHECK_STATE m_check_state;  /* 主状态机当前所处的状态 */
	METHOD m_method;  /* 请求方法 */


	char m_real_file[FILENAME_LEN];  /* 客户请求的目标文件的完整路径，其内容等于doc_root + m_url, doc_root是网站的根目录 */
	char * m_url;   /* 客户请求的目标文件的文件名 */
	char * m_version;  /* HTTP协议版本号, 我们仅支持HTTP/1.1 */
	char * m_host;   /* 主机名 */ 
	int    m_content_length;  /* HTTP请求的消息体的长度 */
	bool m_linger;

	char * m_file_address;  /* 客户请求的目标文件被mmap到内存中的起始位置 */
	struct stat m_file_stat;  /* 目标文件的状态，通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息 */
	
	struct iovec m_iv[2];   /* 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量 */
	int m_iv_count;
};

#endif
