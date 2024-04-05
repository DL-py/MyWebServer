#include "ConnPool.h"
ConnPool::ConnPool(int N, const char * db_name):m_MaxConn(N), m_queuestat(sem(m_MaxConn))
{
      for(int i = 0; i < m_MaxConn; i++)
      {
            MYSQL * conn_ptr = mysql_init(NULL);
            conn_ptr = mysql_real_connect(conn_ptr, "localhost", "root", 
                        "qazwsx789", db_name,  0,  NULL,  CLIENT_FOUND_ROWS);
            if (conn_ptr == NULL)
            {
                  continue;
            }
            mysql_set_character_set(conn_ptr, "utf8");
            m_ConnPool.push_back(conn_ptr);
      }
}
ConnPool::~ConnPool()
{
      for(auto iter = m_ConnPool.begin();  iter != m_ConnPool.end();  iter++)
      {
            MYSQL * conn_ptr = *iter;
            mysql_close(conn_ptr);
      }
}
MYSQL * ConnPool::GetConn()
{
      m_queuestat.wait();   //保证连接池有可以使用的MYSQL链接
      m_queuelocker.lock();
      if(m_ConnPool.empty())
      {
            m_queuelocker.unlock();
            return nullptr;
      }
      MYSQL * ptr = m_ConnPool.front();
      m_ConnPool.pop_front();
      m_queuelocker.unlock();
      return ptr;
}

void ConnPool::PutConn(MYSQL * connPtr)
{             
      m_queuelocker.lock();
      if(m_ConnPool.size() > m_MaxConn)
      {
            m_queuelocker.unlock();
            return ;
      }
      m_ConnPool.push_back(connPtr);
      m_queuestat.post();
      m_queuelocker.unlock();
}