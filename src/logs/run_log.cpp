#include "run_log.hpp"
extern MyConfig globalCFG;
Logger::Logger()
{
    logFD_ = nullptr;
    filePath_ = "./log/rtmsg.log";
    logLevel_ = LogLevel::LOG_INFO;
    maxLogSize_ = 64 * 1024 * 1024; /* 64MB. */
    periodTimeOut_ = 10 * 60;  /* 10 minutes. */
}

Logger::~Logger()
{
    if (logFD_)
    {
        fclose(logFD_);
    }

    pthread_mutex_destroy(&mutex_);
}

bool Logger::initLogger()
{
    const Config& cfg = globalCFG.getConfig();
    try
    {
        if (cfg.exists("log.log_file_name"))
        {
            filePath_ = (const char*)cfg.lookup("log.log_file_name");
        }
        
        if ((logFD_ = fopen(filePath_.c_str(), "w")) == nullptr)
        {
            std::cerr<<"open log file "<<filePath_<<" error: "<<strerror(errno)<<std::endl;
            return false;
        }

        if (cfg.exists("log.log_level"))
        {
            std::string logLevel = cfg.lookup("log.log_level");
            if (! converStringToLogLevel(logLevel))
            {
                std::cerr<<"invalid configration item log.log_level: "<< logLevel << std::endl;
            }
        }
    }
    catch(const SettingTypeException& tex)
    {
        std::cerr<< "invalid configuration type: "<< tex.getPath() << std::endl;
        return false;
    }

    if (!globalCFG.getQuantityValue("log.max_log_size", (int&)maxLogSize_))
    {
        std::cerr<< "invalid configuration type: "<< "log.max_log_size" << std::endl;
        return false;        
    }

    if (!globalCFG.getTimeValue("log.period_timeout", (int&)periodTimeOut_))
    {
        std::cerr<< "invalid configuration type: "<< "log.period_timeout" << std::endl;
        return false;    
    }

    pthread_mutex_init(&mutex_, NULL);
    lastTruncTime_ = time(nullptr);

    return true;
}

void Logger::printLogger()
{
    std::cout << std::left << std::setw(16) << "Logger info: "<<std::endl;
    std::cout << std::left << std::setw(16) << "log file path: " << filePath_ << std::endl;
    try
    {
        std::cout << std::left << std::setw(16) << "log level: " << converLogLevelToString(logLevel_) << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    std::cout << std::left << std::setw(16) << "max log size: " << maxLogSize_ << std::endl;
    std::cout << std::left << std::setw(16) << "timeout period: " << periodTimeOut_ << std::endl;
}

// bool Logger::logRecord(const LogLevel& level, const char* format, ...)
// {
//     va_list args;
//     va_start(args, format);
//     logRecord_(level, __FILE__, __func__, __LINE__, format, args);
//     va_end(args);
// }

#define LOG_DATE_SIZE  64
int logmsg_localtime(char* outtime, int outtime_size)
{
    time_t now = time(NULL);
    struct tm time;
    localtime_r(&now, &time);
    int len = snprintf(outtime, outtime_size, "%4.4d/%2.2d/%2.2d/%2.2d:%2.2d:%2.2d", time.tm_year+1900, time.tm_mon+1, 
            time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
    return len;
}

bool Logger::logRecord_(const LogLevel& level, const char* file, const char* func, int line, const char* format, ...)
{
    static const int maxThreadNameSize = 64;
    if (static_cast<int>(level) > static_cast<int>(logLevel_))
    {
        return true;
    }

    va_list args;
    va_start(args, format);

    pthread_mutex_lock(&mutex_);
    char curTime[LOG_DATE_SIZE];
    int len = logmsg_localtime(curTime, LOG_DATE_SIZE);

    char threadName[maxThreadNameSize];
    pthread_t tid = pthread_self();
    pthread_getname_np(tid, threadName, maxThreadNameSize);

    fprintf(logFD_, "[%s] [%s] [%s|%lu] [%s:%d|%s] ", curTime, converLogLevelToString(level).c_str(), threadName, tid, getFileName(file), line, func);
    vfprintf(logFD_, format, args);
    pthread_mutex_unlock(&mutex_);

    va_end(args);
    fflush(logFD_);
}

bool Logger::converStringToLogLevel(const std::string& logLevel)
{
    if (logLevel == "LOG_EMERG")
    {
        logLevel_ = LogLevel::LOG_EMERG;
        return true;
    }
    else if (logLevel == "LOG_ALERT")
    {
        logLevel_ = LogLevel::LOG_ALERT;
        return true;
    }
    else if (logLevel == "LOG_CRIT")
    {
        logLevel_ = LogLevel::LOG_CRIT;
        return true;
    }
    else if (logLevel == "LOG_ERR")
    {
        logLevel_ = LogLevel::LOG_ERR;
        return true;        
    }
    else if (logLevel == "LOG_WARNING")
    {
        logLevel_ = LogLevel::LOG_WARNING;
        return true;
    }
    else if (logLevel == "LOG_NOTICE")
    {
        logLevel_ = LogLevel::LOG_NOTICE;
        return true;
    }
    else if (logLevel == "LOG_INFO")
    {
        logLevel_ = LogLevel::LOG_INFO;
        return true;
    }
    else if (logLevel == "LOG_DEBUG")
    {
        logLevel_ = LogLevel::LOG_DEBUG;
        return true;
    }
    else
    {
        logLevel_ = LogLevel::LOG_INFO;
        return false;
    }
}

const std::string Logger::converLogLevelToString(const LogLevel& logLevel)
{
    switch(logLevel)
    {
        case LogLevel::LOG_EMERG:
        {
            return "LOG_EMERG";
        }
        case LogLevel::LOG_ALERT:
        {
            return "LOG_ALERT";
        }
        case LogLevel::LOG_CRIT:
        {
            return "LOG_CRIT";
        }
        case LogLevel::LOG_ERR:
        {
            return "LOG_ERR";
        }
        case LogLevel::LOG_WARNING:
        {
            return "LOG_WARNING";
        }
        case LogLevel::LOG_INFO:
        {
            return "LOG_INFO";
        }
        case LogLevel::LOG_DEBUG:
        {
            return "LOG_DEBUG";
        }
        default:
        {
            throw std::invalid_argument("invalid argument: logLevel");
        }
    }
}

const char* Logger::getFileName(const char* filePath)
{
    /**
     *   /www/wwwroot/MyWebServer/handler/handler_simple.cpp
     *   ^                               ^                 ^
     *   |-------------------------------|-----------------|
     * begin                            pos               end
    */
    const char* begin =  filePath;
    const char* end = begin + strlen(filePath) - 1;
    char* pos = (char*)end;
    while(pos >= begin && *pos != '/')
    {
        --pos;
    }

    return (pos >= begin && pos < end) ? (pos + 1) : nullptr;
}