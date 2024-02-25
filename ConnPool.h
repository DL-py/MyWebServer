#ifndef CONN_POOL_H
#define CONN_POOL_H
#include <iostream>
#include <mysql/mysql.h>
#include <list>
#include <assert.h>
#include "locker.h"
using namespace std;
class ConnPool    //创建一个连接池
{
public:
    ConnPool(int N, const char * db_name);
    ~ConnPool();
    MYSQL * GetConn();
    void PutConn(MYSQL * connPtr);
private:
    list<MYSQL *>  m_ConnPool;
    int m_MaxConn;  //连接池中的总的连接数目
    locker m_queuelocker;
    sem m_queuestat;
};

#endif