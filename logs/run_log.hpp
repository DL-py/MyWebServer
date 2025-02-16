#ifndef RUN_LOG_H
#define RUN_LOG_H
#include <stdio.h>
#include <cstring>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <exception>
#include "../config/config.hpp"
enum class LogLevel
{
    LOG_EMERG,
    LOG_ALERT,
    LOG_CRIT,
    LOG_ERR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG
};

class Logger
{
public:
    Logger();
    ~Logger();
private:
    FILE* logFD_;
    std::string filePath_;
    LogLevel logLevel_;
    pthread_mutex_t mutex_;

    uint64_t  maxLogSize_;
    time_t lastTruncTime_;
    time_t periodTimeOut_;
public:
    bool initLogger();
    void printLogger();
    bool logRecord_(const LogLevel& level, const char* file, const char* func, int line, const char* format, ...);

private:
    bool converStringToLogLevel(const std::string& logLevel);
    const std::string converLogLevelToString(const LogLevel& logLevel);

    const char* getFileName(const char* filePath);
};

#define LogRecord(logger, level, format, ...) (logger).logRecord_((level), __FILE__, __func__, __LINE__, (format), ##__VA_ARGS__)
#endif