#include "run_log.h"
static FILE* fp = NULL;

bool rt_log_init()
{
    fp = fopen(LOG_FILE_PATH, "w+");
    if (fp == NULL)
    {
        printf("log file:%s open failed.\n", LOG_FILE_PATH);
        return false;
    }
    return true;
}

bool rt_log_clean()
{
    fclose(fp);
}

int get_timeinfo(char* buf, uint16_t buf_len)
{
    time_t cur_time = time(NULL);
    struct tm * cur_ltime = localtime(&cur_time);
    int len = snprintf(buf, buf_len, "{%d/%d/%d %d:%d:%d}", (cur_ltime->tm_year+1900), 
        (cur_ltime->tm_mon+1), cur_ltime->tm_mday, cur_ltime->tm_hour, cur_ltime->tm_min, cur_ltime->tm_sec);
    return len;
}

void rt_logging(const char* error, uint8_t log_type)
{
    char err_buf[RUNLOG_LENGTH_MAX];
    uint16_t err_buf_len = 0;

    switch (log_type)
    {
        case RUNLOG_INFO:
        {
            err_buf_len += snprintf(err_buf, RUNLOG_LENGTH_MAX - err_buf_len, "[Info]");
            break;
        }
        case RUNLOG_WARNING:
        {
            err_buf_len += snprintf(err_buf, RUNLOG_LENGTH_MAX - err_buf_len, "[Warning]");
            break;
        }
        case RUNLOG_ERROR:
        {
            err_buf_len += snprintf(err_buf, RUNLOG_LENGTH_MAX - err_buf_len, "[Error]");
            break;
        }
        default:
        {
            break;
        }    
    }

    err_buf_len += get_timeinfo(err_buf + err_buf_len, RUNLOG_LENGTH_MAX - err_buf_len);

    err_buf_len += snprintf(err_buf + err_buf_len, RUNLOG_LENGTH_MAX - err_buf_len, " %s.\n", error);

    if (fwrite(err_buf, err_buf_len, 1, fp) != 1)
    {
        printf("run time logging failed ! \n");
    }
    else
    {
        fflush(fp);
    }
}

void rt_fmt_logging(int type, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char err_buf[RUNLOG_LENGTH_MAX];

    int len = vsnprintf(err_buf, RUNLOG_LENGTH_MAX, fmt, args);
    if (len <= 0 || len >= RUNLOG_LENGTH_MAX)
    {
        printf("rt_fmt_logging error.\n");
    }

    va_end(args);
    rt_logging(err_buf, type);
}

// int main()
// {
//     rt_log_init();
//     rt_fmt_logging(RUNLOG_ERROR, "function %s is failed: argv a is %d", "rt_fmt_logging", 20);
//     rt_log_clean();
// }