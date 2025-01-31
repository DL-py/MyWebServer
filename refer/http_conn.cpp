#include "http_conn.h"
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

void http_conn::init( int sockfd, const sockaddr_in& addr )
{
    m_sockfd = sockfd;
    m_address = addr;

    int error = 0;
    socklen_t len = sizeof( error );
    getsockopt( m_sockfd, SOL_SOCKET, SO_ERROR, &error, &len );
    int reuse = 1;
    setsockopt( m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
    addfd( m_epollfd, sockfd, true );
    m_user_count++;

    // initialize read_buf, write_buf, real_file
    memset( m_read_buf, '\0', READ_BUFFER_SIZE );
    memset( m_write_buf, '\0', WRITE_BUFFER_SIZE );
    m_check_state = http_com::CHECK_STATE_REQUESTLINE;

    // initialize m_http_com, m_http_req, m_http_resp
    m_http_com.init();
    m_http_req.init(m_read_buf, READ_BUFFER_SIZE);
    m_http_resp.init(m_write_buf, WRITE_BUFFER_SIZE);
}

void http_conn::reset()
{
    memset( m_read_buf, '\0', READ_BUFFER_SIZE );
    memset( m_write_buf, '\0', WRITE_BUFFER_SIZE );
    m_check_state = http_com::CHECK_STATE_REQUESTLINE;

    m_http_com.reset();
    m_http_req.reset(m_read_buf, READ_BUFFER_SIZE);
    m_http_resp.reset(m_write_buf, WRITE_BUFFER_SIZE);
}

void http_conn::close_conn( bool real_close )
{
    if( real_close && ( m_sockfd != -1 ) )
    {
        removefd( m_epollfd, m_sockfd );
        m_sockfd = -1;
        m_user_count--;
    }
}

bool http_conn::buffer_read()
{
    if (! m_http_req.is_renewable())
    {
        return false;
    }

    char* pos_read = m_http_req.get_read_pos();
    int nbytes_left = m_http_req.nbytes_left();

    int bytes_read = 0;
    while( true )
    {
        bytes_read = recv( m_sockfd, pos_read, nbytes_left, 0 );
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

        m_http_req.update_read_pos(bytes_read);

        pos_read = m_http_req.get_read_pos();
        nbytes_left = m_http_req.nbytes_left();
    }
    return true;
}

bool http_conn::buffer_write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = 0;

    for (int i = 0; i < m_iv_count; i++)
    {
        bytes_to_send += int(m_iv[i].iov_len);
    }

    if ( bytes_to_send == 0 )
    {
        modfd( m_epollfd, m_sockfd, EPOLLIN );
        reset();
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
//            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;

        if ( bytes_to_send <= bytes_have_send )
        {
//            unmap();
            bool linger = m_http_req.get_header_linger();
            if( linger )
            {
                reset();
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

void http_conn::process()
{
    http_com::HTTP_CODE read_ret = process_read();
    if ( read_ret == http_com::NO_REQUEST )
    {
        modfd( m_epollfd, m_sockfd, EPOLLIN );
        return;
    }

    char* url = m_http_req.m_url;
    m_http_handler = http_handler_produce(read_ret, m_http_req.m_method, url, m_http_req.m_content, m_http_req.m_content_length);

    read_ret = m_http_handler->req_handler_pre();
    read_ret = m_http_handler->req_handler();

    bool write_ret = process_write( read_ret );
    if ( ! write_ret )
    {
        close_conn();
    }

//    m_http_handler->req_handler_post();
    modfd( m_epollfd, m_sockfd, EPOLLOUT );
}

http_com::HTTP_CODE http_conn::process_read()
{
    http_com::LINE_STATUS line_status = http_com::LINE_OK;
    http_com::HTTP_CODE ret = http_com::NO_REQUEST;
    char* text = 0;

    while ( ( ( m_check_state == http_com::CHECK_STATE_CONTENT ) && ( line_status == http_com::LINE_OK  ) )
            || ( ( line_status = m_http_req.parse_line() ) == http_com::LINE_OK ) )
    {
        text = m_http_req.get_line();
        int checked_idx = m_http_req.get_checked_idx();
        m_http_req.set_start_line(checked_idx);

        switch ( m_check_state )
        {
            case http_com::CHECK_STATE_REQUESTLINE:
            {
                ret = m_http_req.parse_request_line( text );
                m_check_state = http_com::CHECK_STATE_HEADER;

                if ( ret == http_com::BAD_REQUEST )
                {
                    return http_com::BAD_REQUEST;
                }
                break;
            }
            case http_com::CHECK_STATE_HEADER:
            {
                if (text[0] != '\0')
                {
                    ret = m_http_req.parse_headers( text );
                }
                else
                {
                    int content_length = m_http_req.get_header_content_length();
                    if (content_length != 0)
                    {
                        ret = http_com::NO_REQUEST;
                        m_check_state = http_com::CHECK_STATE_CONTENT;
                    }
                    else
                    {
                        ret = http_com::GET_REQUEST;
                    }
                }

                if ( ret == http_com::BAD_REQUEST )
                {
                    return http_com::BAD_REQUEST;
                }
                else if ( ret == http_com::GET_REQUEST )
                {
                    if(m_http_req.m_method == http_com::GET)
                    {
                        return http_com::GET_REQUEST;
                    }
                    if(m_http_req.m_method == http_com::POST)
                    {
                        return http_com::FILE_REQUEST;
                    }
                }
                break;
            }
            case http_com::CHECK_STATE_CONTENT:
            {
                ret = m_http_req.parse_content( text );
                if ( ret == http_com::GET_REQUEST )
                {
                    if(m_http_req.m_method == http_com::GET)
                    {
                        return http_com::GET_REQUEST;
                    }
                    if(m_http_req.m_method == http_com::POST)
                    {
                        return http_com::GET_REQUEST;
                    }
                }
                line_status = http_com::LINE_OPEN;
                break;
            }
            default:
            {
                return http_com::INTERNAL_ERROR;
            }
        }
    }
    return http_com::NO_REQUEST;
}

bool http_conn::process_write( http_com::HTTP_CODE ret )
{
    switch ( ret )
    {
        case http_com::INTERNAL_ERROR:
        {
            m_http_resp.add_status_line( 500, m_http_com.error_500_title );
            m_http_resp.add_headers( strlen( m_http_com.error_500_form ), m_http_req.m_linger );
            if ( ! m_http_resp.add_content( m_http_com.error_500_form ) )
            {
                return false;
            }
            break;
        }
        case http_com::BAD_REQUEST:
        {
            m_http_resp.add_status_line( 400, m_http_com.error_400_title );
            m_http_resp.add_headers( strlen( m_http_com.error_400_form ), m_http_req.m_linger);
            if ( ! m_http_resp.add_content( m_http_com.error_400_form ) )
            {
                return false;
            }
            break;
        }
        case http_com::NO_RESOURCE:
        {
            m_http_resp.add_status_line( 404, m_http_com.error_404_title );
            m_http_resp.add_headers( strlen( m_http_com.error_404_form ), m_http_req.m_linger);
            if ( ! m_http_resp.add_content( m_http_com.error_404_form ) )
            {
                return false;
            }
            break;
        }
        case http_com::FORBIDDEN_REQUEST:
        {
            m_http_resp.add_status_line( 403, m_http_com.error_403_title );
            m_http_resp.add_headers( strlen( m_http_com.error_403_form ), m_http_req.m_linger);
            if ( ! m_http_resp.add_content( m_http_com.error_403_form ) )
            {
                return false;
            }
            break;
        }
        case http_com::FILE_REQUEST:  /**get request*/
        {
            m_http_resp.add_status_line( 200, m_http_com.ok_200_title );

            char* handler_buff = m_http_handler->get_handler_buffer();
            int handler_buff_len = m_http_handler->get_handler_buffer_len();

            bool linger = m_http_req.get_header_linger();

            m_http_resp.add_headers(handler_buff_len, linger);

            char* write_buff = m_http_resp.get_buffer();
            int write_buff_len = m_http_resp.get_buffer_idx();

            m_iv[ 0 ].iov_base = write_buff;
            m_iv[ 0 ].iov_len = write_buff_len;
            m_iv[ 1 ].iov_base = handler_buff;
            m_iv[ 1 ].iov_len = handler_buff_len;
            m_iv_count = 2;
            return true;

//            if ( m_http_req.m_method == http_com::GET && m_file_stat.st_size != 0 )
//            {
//                m_http_resp.add_status_line( 200, m_http_com.ok_200_title );
//                add_headers( m_file_stat.st_size );
//                m_iv[ 0 ].iov_base = m_write_buf;
//                m_iv[ 0 ].iov_len = m_write_idx;
//                m_iv[ 1 ].iov_base = m_file_address;
//                m_iv[ 1 ].iov_len = m_file_stat.st_size;
//                m_iv_count = 2;
//                return true;
//            }
//            else if(m_http_req.m_method == http_com::POST)
//            {
//                m_http_resp.add_status_line( 200,  m_http_com.ok_200_title );
//                m_http_resp.add_linger(m_linger);
//
//                if(string(m_url) == "/register.cgi")
//                {
//                    execute_register_cgi();
//                    rt_fmt_logging(RUNLOG_INFO, "html:\n%s", m_write_buf);
//                }
//
//                if(string(m_url) == "/login.cgi" )  //登录操作
//                    execute_login_cgi();
//                break;
//            }
//            else
//            {
//                const char* ok_string = "<html><body></body></html>";
//                add_headers( strlen( ok_string ) );
//                if ( ! m_http_resp.add_content( ok_string ) )
//                {
//                    return false;
//                }
//            }
        }
        default:
        {
            return false;
        }
    }

//    m_iv[ 0 ].iov_base = m_write_buf;
//    m_iv[ 0 ].iov_len = m_write_idx;
//    m_iv_count = 1;
    return true;
}

