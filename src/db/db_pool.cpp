#include "db_pool.h"

#include <stdexcept>

DbPool::DbPool(int size, DbCreateFn create, DbDestroyFn destroy, bool pingOnAcquire)
    : create_(create), destroy_(destroy), pingOnAcquire_(pingOnAcquire),
      capacity_(size), closed_(false)
{
    if (size <= 0)
    {
        throw std::runtime_error("db pool size must be positive");
    }
    try
    {
        for (int i = 0; i < size; ++i)
        {
            MYSQL* conn = create_();
            if (!conn)
            {
                throw std::runtime_error("db connection create returned null");
            }
            available_.push_back(conn);
        }
    }
    catch (...)
    {
        for (MYSQL* conn : available_)
        {
            destroy_(conn);
        }
        available_.clear();
        throw;
    }
}

DbPool::~DbPool()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }
    cond_.notify_all();
    for (MYSQL* conn : available_)
    {
        destroy_(conn);
    }
    available_.clear();
}

MYSQL* DbPool::acquire()
{
    mysql_thread_init();
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this]() { return !available_.empty() || closed_; });
    if (closed_)
    {
        return nullptr;
    }
    MYSQL* conn = available_.front();
    available_.pop_front();
    lock.unlock();

    if (pingOnAcquire_ && mysql_ping(conn) != 0)
    {
        destroy_(conn);
        try
        {
            conn = create_();
        }
        catch (...)
        {
            lock.lock();
            closed_ = true;
            lock.unlock();
            cond_.notify_all();
            throw;
        }
        if (!conn)
        {
            lock.lock();
            closed_ = true;
            lock.unlock();
            cond_.notify_all();
            throw std::runtime_error("db reconnect returned null");
        }
    }
    return conn;
}

void DbPool::release(MYSQL* conn)
{
    if (!conn)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_)
        {
            destroy_(conn);
            return;
        }
        available_.push_back(conn);
    }
    cond_.notify_one();
}

int DbPool::capacity() const
{
    return capacity_;
}

DbPool* makeMysqlPool(const DbConfig& config)
{
    if (config.poolSize <= 0)
    {
        throw std::runtime_error("db pool size must be positive");
    }
    DbCreateFn create = [&config]() -> MYSQL* {
        MYSQL* conn = mysql_init(nullptr);
        if (!conn)
        {
            throw std::runtime_error("mysql_init failed");
        }
        if (mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4") != 0)
        {
            mysql_close(conn);
            throw std::runtime_error("mysql_options charset failed");
        }
        if (!mysql_real_connect(conn, config.host.c_str(), config.user.c_str(),
                                config.password.c_str(), config.name.c_str(),
                                config.port, nullptr, CLIENT_FOUND_ROWS))
        {
            std::string err = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("mysql_real_connect failed: " + err);
        }
        return conn;
    };
    DbDestroyFn destroy = [](MYSQL* conn) { mysql_close(conn); };
    return new DbPool(config.poolSize, create, destroy, true);
}
