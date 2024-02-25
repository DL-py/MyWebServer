#include "http_conn.h"

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";
const char* doc_root = "html";

ConnPool conn_pool(5, "users");

int setnonblocking( int fd ){
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void addfd( int epollfd, int fd, bool one_shot ){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if( one_shot )
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}

void removefd( int epollfd, int fd ){
    epoll_ctl( epollfd, EPOLL_CTL_DEL, fd, 0 );
    close( fd );
}

void modfd( int epollfd, int fd, int ev ){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl( epollfd, EPOLL_CTL_MOD, fd, &event );
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

std::string UrlEncode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (isalnum((unsigned char)str[i]) ||
            (str[i] == '-') ||
            (str[i] == '_') ||
            (str[i] == '.') ||
            (str[i] == '~'))
            strTemp += str[i];
        else if (str[i] == ' ')
            strTemp += "+";
        else
        {
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] % 16);
        }
    }
    return strTemp;
}

std::string UrlDecode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (str[i] == '+') strTemp += ' ';
        else if (str[i] == '%')
        {
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[++i]);
            strTemp += high * 16 + low;
        }
        else strTemp += str[i];
    }
    return strTemp;
}
// convert string to wstring
inline std::wstring to_wide_string(const std::string& input)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.from_bytes(input);
}

void http_conn::close_conn( bool real_close ){
    if( real_close && ( m_sockfd != -1 ) )
    {
        removefd( m_epollfd, m_sockfd );
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::init( int sockfd, const sockaddr_in& addr ){
    m_sockfd = sockfd;
    m_address = addr;
    int error = 0;
    socklen_t len = sizeof( error );
    getsockopt( m_sockfd, SOL_SOCKET, SO_ERROR, &error, &len );
    int reuse = 1;
    setsockopt( m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
    addfd( m_epollfd, sockfd, true );
    m_user_count++;

    init();
}

void http_conn::init(){
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;

    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset( m_read_buf, '\0', READ_BUFFER_SIZE );
    memset( m_write_buf, '\0', WRITE_BUFFER_SIZE );
    memset( m_real_file, '\0', FILENAME_LEN );
}
/**
 * @brief parse the http request in m_read_buf character by character
 * @return LINE_OK if find complete line, end with \r\n
 * 
*/
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for ( ; m_checked_idx < m_read_idx; ++m_checked_idx )
    {
        temp = m_read_buf[ m_checked_idx ];
        if ( temp == '\r' )
        {
            if ( ( m_checked_idx + 1 ) == m_read_idx )
            {
                return LINE_OPEN;
            }
            else if ( m_read_buf[ m_checked_idx + 1 ] == '\n' )
            {
                m_read_buf[ m_checked_idx++ ] = '\0';
                m_read_buf[ m_checked_idx++ ] = '\0';
                return LINE_OK;
            }

            return LINE_BAD;
        }
        else if( temp == '\n' )
        {
            if( ( m_checked_idx > 1 ) && ( m_read_buf[ m_checked_idx - 1 ] == '\r' ) )
            {
                m_read_buf[ m_checked_idx-1 ] = '\0';
                m_read_buf[ m_checked_idx++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}

bool http_conn::my_read()  /* read http request from socket and write to m_read_buf */
{
    if( m_read_idx >= READ_BUFFER_SIZE )
    {
        return false;
    }

    int bytes_read = 0;
    while( true )
    {
        bytes_read = recv( m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0 );
        if ( bytes_read == -1 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                break;
            }
            return false;
        }
        else if ( bytes_read == 0 )
        {
            return false;
        }

        m_read_idx += bytes_read;
    }
    return true;
}

http_conn::HTTP_CODE http_conn::parse_request_line( char* text ){
        m_url = strpbrk( text, " \t" );
        if ( ! m_url )
        {
            return BAD_REQUEST;
        }
        *m_url++ = '\0';

        char* method = text;
        if ( strcasecmp( method, "GET" ) == 0 )
        {
            m_method = GET;
        }
        else if( strcasecmp(method, "POST") == 0)
        {
            m_method = POST;
        }
        else
        {
            return BAD_REQUEST;
        }

        m_url += strspn( m_url, " \t" );
        m_version = strpbrk( m_url, " \t" );
        if ( ! m_version )
        {
            return BAD_REQUEST;
        }
        *m_version++ = '\0';
        m_version += strspn( m_version, " \t" );
        if ( strcasecmp( m_version, "HTTP/1.1" ) != 0 )
        {
            return BAD_REQUEST;
        }
        /* 如果请求行中没有http:// 比如请求行直接为 GET /test.html HTTP/1.1 */
        /* 此时，m_url = "/test.html" */
        if ( strncasecmp( m_url, "http://", 7 ) == 0 )
        {
            m_url += 7;
            m_url = strchr( m_url, '/' );
        }

        if ( ! m_url || m_url[ 0 ] != '/' )
        {
            return BAD_REQUEST;
        }

        m_check_state = CHECK_STATE_HEADER;
        return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers( char* text )
{
    if( text[ 0 ] == '\0' )
    {
        if ( m_method == HEAD )
        {
            return GET_REQUEST;
        }

        if ( m_content_length != 0 )
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }

        return GET_REQUEST;
    }
    else if ( strncasecmp( text, "Connection:", 11 ) == 0 )
    {
        text += 11;
        text += strspn( text, " \t" );
        if ( strcasecmp( text, "keep-alive" ) == 0 )
        {
            m_linger = true;
        }
    }
    else if ( strncasecmp( text, "Content-Length:", 15 ) == 0 )
    {
        text += 15;
        text += strspn( text, " \t" );
        m_content_length = atol( text );
    }
    else if ( strncasecmp( text, "Host:", 5 ) == 0 )
    {
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
    }
    else
    {
        //printf( "oop! unknow header %s\n", text );
    }

    return NO_REQUEST;

}

http_conn::HTTP_CODE http_conn::parse_content( char* text ){
    if ( m_read_idx >= ( m_content_length + m_checked_idx ) )
    {
        text[ m_content_length ] = '\0';
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while ( ( ( m_check_state == CHECK_STATE_CONTENT ) && ( line_status == LINE_OK  ) )
                || ( ( line_status = parse_line() ) == LINE_OK ) )
    {
        text = get_line();
        m_start_line = m_checked_idx;

        switch ( m_check_state )
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line( text );
                if ( ret == BAD_REQUEST )
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers( text );
                if ( ret == BAD_REQUEST )
                {
                    return BAD_REQUEST;
                }
                else if ( ret == GET_REQUEST )
                {
                    if(m_method == GET)
                        return do_request();
                    if(m_method == POST)
                    {
                       return FILE_REQUEST;
                    }
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content( text );
                if ( ret == GET_REQUEST )
                {
                    if(m_method == GET)
                        return do_request();
                    if(m_method == POST)
                    {
                       return FILE_REQUEST;
                    }
                        
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request(){
        strcpy( m_real_file, doc_root );
        int len = strlen( doc_root );
        strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
        if ( stat( m_real_file, &m_file_stat ) < 0 )
        {
                return NO_RESOURCE;
        }

        if ( ! ( m_file_stat.st_mode & S_IROTH ) )
        {
                return FORBIDDEN_REQUEST;
        }

        if ( S_ISDIR( m_file_stat.st_mode ) )
        {
                return BAD_REQUEST;
        }

        int fd = open( m_real_file, O_RDONLY );
        m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
        
        close( fd );
        return FILE_REQUEST;
}

void http_conn::execute_register_cgi(){
    /*parse post form*/
    char * form = get_line();

    /*clone post form*/
    int len = strlen(form);  
    char* buffer = (char* )malloc(len + 1);
    if (buffer == NULL)
    {
        return ;
    }

    snprintf(buffer, len + 1, "%s", form);

    char* ptr = buffer;
    while(*ptr != '\0')
    {
        if (*ptr == '&')
        {
            *ptr = ' ';
        }
        ptr++;
    }

    struct information info;
    int num = sscanf(buffer, "user=%s password=%s hobby=%s sex=%s introduction=%s address=%s", info.account, \
                                            info.password, info.hobby, info.sex, info.introduction, info.address);

    struct information decoded_info;
    decode_user_infomation(&info, &decoded_info);
    
    /* MySQL operation */
    MYSQL * conn_ptr = conn_pool.GetConn();
    bool success = false;
    if (!user_already_exist(conn_ptr, decoded_info.account))
    {
        success = insert_user_information(conn_ptr, &decoded_info);
    }
    conn_pool.PutConn(conn_ptr);
    
    char * text = get_line();
    snprintf(m_real_file, FILENAME_LEN, "%s%s", doc_root, m_url);
    
    if (exec_register_cgi(m_real_file, text, m_content_length, success) == false)
    {
        
    }

    
    int pipe_parent2child[2];
    int pipe_child2parent[2];
    int ret, status;
    char buf[255];

    ret = pipe(pipe_parent2child);
    assert(ret >= 0);
    ret = pipe(pipe_child2parent);
    assert(ret >= 0);
    int childfd = fork();
    assert(childfd >= 0);
    if(childfd == 0)   /* 子进程 */
    {
        char length_env[255];
        char repeat_env[255];
        close(pipe_parent2child[1]);
        close(pipe_child2parent[0]);

        dup2(pipe_parent2child[0], 0);
        dup2(pipe_child2parent[1], 1);

        sprintf(length_env, "CONTENT_LENGTH=%d", m_content_length);
        sprintf(repeat_env, "Is_Repeat=%d",  !success);
        putenv(length_env);
        putenv(repeat_env);
        strcpy(m_real_file, doc_root);
        int len = strlen( doc_root );
        strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
        execl(m_real_file, m_real_file, NULL);  /* 执行CGI脚本 */
        printf("child process finish!\n");
        exit(0);
    }
    else
    {
        close(pipe_parent2child[0]);
        close(pipe_child2parent[1]);

        char * text = get_line();
        // printf("text is :\n%s\n", text);
        write(pipe_parent2child[1], text, m_content_length);
        add_content_length(240);
        add_response("%s", "Transfer-Encoding: chunked\r\n");
        add_response("%s", "Content-type:text/html\r\n");
        add_blank_line();
        char tmp_buffer[1024];
        memset( tmp_buffer, '\0', sizeof(tmp_buffer) );
        int ret = read(pipe_child2parent[0], tmp_buffer, sizeof(tmp_buffer));
        while(ret > 0)
        {
            add_response("%0x\r\n",ret);
            add_response("%s", tmp_buffer);
            add_blank_line();
            printf("tmp_buffer is:\n%s", tmp_buffer);	
            memset( tmp_buffer, '\0', sizeof(tmp_buffer) );
            ret = read(pipe_child2parent[0], tmp_buffer, sizeof(tmp_buffer));
        }
        add_response("%0x\r\n",0);
        add_blank_line();
        close(pipe_parent2child[1]);
        close(pipe_child2parent[0]);
        waitpid(childfd, &status, 0);
    }
}

void http_conn::execute_login_cgi()
{  
    //执行登录操作：
    string input = get_line();
    //找到用户名和密码：
    int idx =  input.find('&');
    string user_item = input.substr(0, idx);
    string password_item = input.substr(idx+1, input.size()-idx-1);
    int user_idx  = user_item.find('=');
    int password_idx = password_item.find('=');
    string user = user_item.substr(user_idx+1,  user_item.size()-(user_idx+1));
    string password = password_item.substr(password_idx+1,  password_item.size()-(password_idx+1));
    //根据用户名到数据库中寻找密码并与用户输入的密码进行对比：
    MYSQL * conn_ptr = conn_pool.GetConn();
    string sql_cmd = "select password from information where user = \'" + user+"\';";
    cout<<"sql_cmd: "<<sql_cmd<<endl;
    mysql_query(conn_ptr, sql_cmd.c_str());
    MYSQL_RES *result = mysql_store_result(conn_ptr);
    int row_count = mysql_num_rows(result);
    int is_correct = 0;
    string output = "";
    if(row_count == 1 )
    {
        MYSQL_ROW row = mysql_fetch_row(result);
        string real_password = row[0];
        if( real_password == password)
        {
            is_correct = 1;
            cout<<"real passwod is  "<<real_password<<endl;
            //read user information from MySQL and process by child process
            sql_cmd = "select * from information where user = \'" + user+"\';";
            mysql_query(conn_ptr, sql_cmd.c_str());  //get user information
            MYSQL_RES * result = mysql_store_result(conn_ptr);
            MYSQL_ROW row  = mysql_fetch_row(result);
            int field_count = mysql_num_fields(result);
            for(int i = 1; i < field_count; i ++)
            {
                MYSQL_FIELD *field = NULL;
                field = mysql_fetch_field_direct(result, i);
                output += string(field->name) + "=" + row[i]+"&";
            }
            output.pop_back();
        }
        
    }
    conn_pool.PutConn(conn_ptr);
    wstring w_output = to_wide_string(output);
    //利用管道进行父子进程间的通信：
    int pipe_parent2child[2];
    int pipe_child2parent[2];
    int ret, status;
    char buf[255];
    ret = pipe(pipe_parent2child);
    assert(ret >= 0);
    ret = pipe(pipe_child2parent);
    assert(ret >= 0);
    int childfd = fork();
    assert(childfd >= 0);
    if(childfd == 0)   /* 子进程 */
    {
        char length_env[255];
        char IsCorrect[255];
        close(pipe_parent2child[1]);
        close(pipe_child2parent[0]);

        dup2(pipe_parent2child[0], 0);
        dup2(pipe_child2parent[1], 1);
       
        // sprintf(length_env, "CONTENT_LENGTH=%d", (int)output.size());
        sprintf(length_env, "CONTENT_LENGTH=%d", (int) w_output.size());
        sprintf(IsCorrect, "IsCorrect=%d", is_correct);
        putenv(length_env);
        putenv(IsCorrect);
        strcpy(m_real_file, doc_root);
        int len = strlen( doc_root );
        strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
        execl(m_real_file, m_real_file, NULL);  /* 执行CGI脚本 */  
        exit(0);
    }
    else
    {
        close(pipe_parent2child[0]);
        close(pipe_child2parent[1]);
        cout<<"output.c_str():"<<output.c_str()<<endl;
        cout<<"w_output.size():"<<w_output.size()<<endl;
        write(pipe_parent2child[1], output.c_str(),  output.size());
        add_content_length(240);
        add_response("%s", "Transfer-Encoding: chunked\r\n");
        add_response("%s", "Content-type:text/html\r\n");
        add_blank_line();
        char tmp_buffer[1024];
        memset( tmp_buffer, '\0', sizeof(tmp_buffer) );
        int ret = read(pipe_child2parent[0], tmp_buffer, sizeof(tmp_buffer));
        while(ret > 0)
        {
            add_response("%0x\r\n",ret);
            add_response("%s", tmp_buffer);
            printf("tmp_buffer is:\n%s", tmp_buffer);	
            memset( tmp_buffer, '\0', sizeof(tmp_buffer) );
            ret = read(pipe_child2parent[0], tmp_buffer, sizeof(tmp_buffer));
        }
        add_response("%0x\r\n",0);
        add_blank_line();
        close(pipe_parent2child[1]);
        close(pipe_child2parent[0]);
        waitpid(childfd, &status, 0);
    }
}

void http_conn::unmap()
{
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = 0;
    }
}
/**
 * @brief write http response to client
 * 
*/
bool http_conn::my_write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;

    if ( bytes_to_send == 0 )
    {
        modfd( m_epollfd, m_sockfd, EPOLLIN );
        init();
        return true;
    }

    while( 1 )
    {
        temp = writev( m_sockfd, m_iv, m_iv_count );
        if ( temp <= -1 )
        {
            if( errno == EAGAIN )
            {
                modfd( m_epollfd, m_sockfd, EPOLLOUT );
                return true;
            }
            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;

        if ( bytes_to_send <= bytes_have_send )
        {
            unmap();
            if( m_linger )
            {
                init();
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return true;
            }
            else
            {
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return false;
            } 
        }
    }
}

bool http_conn::add_response( const char* format, ... ){
    if( m_write_idx >= WRITE_BUFFER_SIZE )
    {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) )
    {
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    return true;
}

bool http_conn::add_status_line( int status, const char* title ){
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

bool http_conn::add_headers( int content_len ){
    add_content_length( content_len );
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length( int content_len ){
    return add_response( "Content-Length: %d\r\n", content_len );
}

bool http_conn::add_linger(){
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}

bool http_conn::add_blank_line(){
    return add_response( "%s", "\r\n" );
}

bool http_conn::add_content( const char* content ){
    return add_response( "%s", content );
}

bool http_conn::process_write( HTTP_CODE ret ){
    switch ( ret )
    {
        case INTERNAL_ERROR:
        {
            add_status_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) )
            {
                return false;
            }
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) )
            {
                return false;
            }
            break;
        }
        case NO_RESOURCE:
        {
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) )
            {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line( 403, error_403_title );
            add_headers( strlen( error_403_form ) );
            if ( ! add_content( error_403_form ) )
            {
                return false;
            }
            break;
        }
        case FILE_REQUEST:  /**get request*/
        {
            if ( m_method == GET && m_file_stat.st_size != 0 )
            {
                add_status_line( 200, ok_200_title );
                add_headers( m_file_stat.st_size );
                m_iv[ 0 ].iov_base = m_write_buf;
                m_iv[ 0 ].iov_len = m_write_idx;
                m_iv[ 1 ].iov_base = m_file_address;
                m_iv[ 1 ].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            }
            else if(m_method == POST)
            {
                add_status_line( 200,  ok_200_title );
                add_linger();
                
                if(string(m_url) == "/register.cgi")
                {
                    execute_register_cgi();
                    rt_fmt_logging(RUNLOG_INFO, "html:\n%s", m_write_buf);
                }
                    
                if(string(m_url) == "/login.cgi" )  //登录操作
                   execute_login_cgi();
                break;
            }
            else
            {
                const char* ok_string = "<html><body></body></html>";
                add_headers( strlen( ok_string ) );
                if ( ! add_content( ok_string ) )
                {
                    return false;
                }
            }
        }
        default:
        {
            return false;
        }
    }
    
    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

void http_conn::process(){
    HTTP_CODE read_ret = process_read();
    if ( read_ret == NO_REQUEST )
    {
        modfd( m_epollfd, m_sockfd, EPOLLIN );
        return;
    }

    bool write_ret = process_write( read_ret );
    if ( ! write_ret )
    {
        close_conn();
    }

    modfd( m_epollfd, m_sockfd, EPOLLOUT );
}