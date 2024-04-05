//
// Created by dl on 3/14/24.
//

#include "http_handler_cgi.h"

const char* http_handler_cgi::doc_root = "cgi";

http_handler_cgi::http_handler_cgi(char* url, char* content, int content_length):http_handler()
{
    m_ret_code = http_com::GET_REQUEST;
    int len = snprintf(m_real_file, FILENAME_LEN, "%s%s", doc_root, url);
    if (len >= FILENAME_LEN || len < 0)
    {
        m_ret_code = http_com::BAD_REQUEST;
    }

    m_content = content;
    m_content_len = content_length;

    m_handler_buf = new char[MAX_HANDLER_BUFFER_SIZE];
    
}

http_handler_cgi::~http_handler_cgi()
{
    delete m_handler_buf;
}


http_com::HTTP_CODE http_handler_cgi::req_handler_pre()
{
    if (m_ret_code != http_com::GET_REQUEST)
    {
        return m_ret_code;
    }

    if ( stat( m_real_file, &m_file_stat ) < 0 )
    {
        m_ret_code = http_com::NO_RESOURCE;
        return m_ret_code;
    }

    // ordinary file check
    if ( ! S_ISREG(m_file_stat.st_mode) )
    {
        m_ret_code = http_com::BAD_REQUEST;
        return m_ret_code;
    }

    // executable check
    if ( ! ( m_file_stat.st_mode & S_IXOTH ) )
    {
        m_ret_code = http_com::FORBIDDEN_REQUEST;
        return m_ret_code;
    }

    return m_ret_code;
}

http_com::HTTP_CODE http_handler_cgi::req_handler()
{
    /* create two pipes for communication parent and child*/
    int pipe_p2c[2];
    int pipe_c2p[2];

    if (pipe(pipe_p2c) < 0)
    {
        return http_com::INTERNAL_ERROR;
    }

    if (pipe(pipe_c2p) < 0)
    {
        close(pipe_p2c[0]);
        close(pipe_p2c[1]);
        return http_com::INTERNAL_ERROR;
    }

    /*create a child process*/
    int child_pid = fork();
    if (child_pid < 0)
    {
        return http_com::INTERNAL_ERROR;
    }

    if (child_pid == 0)
    {
        // child process
        close(pipe_p2c[1]);
        close(pipe_c2p[0]);

        dup2(pipe_p2c[0], 0);
        dup2(pipe_c2p[1], 1);

        char m_content_len_str[16];
        snprintf(m_content_len_str, sizeof(m_content_len_str), "%d", m_content_len); 
        execl(m_real_file, m_real_file, m_content_len_str, nullptr);
        exit(0);
    }
    else
    {
        // parent process
        close(pipe_p2c[0]);
        close(pipe_c2p[1]);

        write(pipe_p2c[1], m_content, m_content_len);

        char buffer[MAX_BUFFER_SIZE] = {0};
        int nread = 0;

        while((nread = read(pipe_c2p[0], buffer, sizeof(buffer))) > 0)
        {
            memcpy(m_handler_buf + m_handler_buf_len, buffer, nread);
            memset(buffer, 0, MAX_BUFFER_SIZE);
            m_handler_buf_len += nread;
        }

        close(pipe_p2c[1]);
        close(pipe_c2p[0]);

        int status;
        waitpid(child_pid, &status, 0);

        if (status != 0)
        {
            m_ret_code = http_com::INTERNAL_ERROR;
        }
        else
        {
            m_ret_code = http_com::FILE_REQUEST;
        }

        return m_ret_code;
    }
}

http_com::HTTP_CODE http_handler_cgi::req_handler_post()
{

}