#ifndef DB_DB_POOL_H
#define DB_DB_POOL_H
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <mysql.h>

#if MYSQL_VERSION_ID >= 80000
using MySqlBool = bool;
#else
using MySqlBool = my_bool;
#endif

struct DbConfig {
    std::string host;
    int port;
    std::string user;
    std::string password;
    std::string name;
    int poolSize;
};

using DbCreateFn = std::function<MYSQL*()>;
using DbDestroyFn = std::function<void(MYSQL*)>;

class DbPool {
public:
    DbPool(int size, DbCreateFn create, DbDestroyFn destroy, bool pingOnAcquire = true);
    ~DbPool();
    MYSQL* acquire();
    void release(MYSQL* conn);
    int capacity() const;
private:
    std::list<MYSQL*> available_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    DbCreateFn create_;
    DbDestroyFn destroy_;
    bool pingOnAcquire_;
    int capacity_;
    bool closed_;
};

DbPool* makeMysqlPool(const DbConfig& config);
#endif
