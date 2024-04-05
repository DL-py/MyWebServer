#ifndef RUN_LOG_H
#define RUN_LOG_H
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#define RUNLOG_INFO     0
#define RUNLOG_WARNING  1
#define RUNLOG_ERROR    2

#define LOG_FILE_PATH  "./run_time.log"

#define RUNLOG_LENGTH_MAX 65536 


bool rt_log_init();
bool rt_log_clean();
void rt_logging(const char* error, uint8_t log_type);
void rt_fmt_logging(int type, const char* fmt, ...);
#endif