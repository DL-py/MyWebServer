#include "x_file.h"


bool x_is_file_executable(const char* filename)
{
    struct stat file_stat;
    if (!filename || stat(filename, &file_stat) < 0)
    {
        rt_fmt_logging(RUNLOG_ERROR, "get file %s state error.", filename);
        return false;
    }

    if (!S_ISREG(file_stat.st_mode))
    {
        rt_fmt_logging(RUNLOG_ERROR, "file %s is not a regular file.", filename);
        return false;
    }

    return access(filename, R_OK|X_OK) == 0;
}
