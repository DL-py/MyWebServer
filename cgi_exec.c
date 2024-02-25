#include "cgi_exec.h"

bool exec_register_cgi(const char* cgi_path, char* text, int text_len, bool success)
{
    int cgi_len = strlen(cgi_path);
    if (!cgi_path || cgi_len <= 4 || strncmp(cgi_path + (cgi_len - 4), ".cgi", 4) != 0)
    {
        return false;
    }

    /* create two pipes for communication parent and child*/
    int pipe_p2c[2];
    int pipe_c2p[2];

    if (pipe(pipe_p2c) < 0)
    {
        return false;
    }

    if (pipe(pipe_c2p) < 0)
    {
        close(pipe_p2c[0]);
        close(pipe_p2c[1]);
    }

    /*create a child process*/
    int child_pid = fork();
    if (child_pid < 0)
    {
        return false;
    }

    int status = 0;
    if (child_pid == 0)
    {
        close(pipe_p2c[1]);
        close(pipe_c2p[0]);

        // dup2(pipe_c2p[1], 1);  /*redirection*/
        dup2(pipe_p2c[0], 0);

        if (!is_file_executable(cgi_path))
        {
            rt_fmt_logging(RUNLOG_ERROR, "filename %s is not executable.", cgi_path);
        }

        char length_env[64];
        char repeat_env[64];
        snprintf(length_env, sizeof(length_env), "CONTENT_LENGTH=%d", text_len);
        snprintf(repeat_env, sizeof(repeat_env), "Is_Repeat=%d",  !success);
        putenv(length_env);
        putenv(repeat_env);

        execl(cgi_path, cgi_path, NULL);
        exit(0);
    }
    else
    {
        close(pipe_p2c[0]);
        close(pipe_c2p[1]);

        write(pipe_p2c[1], text, text_len);
        char response[MAX_RESPONSE_SIZE];
        int len_response = 0;

        char buffer[MAX_BUFFER_SIZE] = {0};
        int nread = 0;
        while((nread = read(pipe_c2p[0], buffer, sizeof(buffer))) > 0)
        {
            memcpy(response, buffer, nread);
            memset(buffer, 0, MAX_BUFFER_SIZE);
            len_response += nread;
        }

        close(pipe_p2c[1]);
        close(pipe_c2p[0]);
        waitpid(child_pid, &status, 0);
    }

    return true;

}