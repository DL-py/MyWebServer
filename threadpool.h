#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <atomic>
#include <exception>
#include <iostream>
#include <pthread.h>
#include "locker.h"

template< typename T >
class threadpool
{
public:
    threadpool( int thread_number = 8, int max_requests = 10000 );
    ~threadpool();
    bool append( T* request );

private:
    static void* worker( void* arg );
    void run();
    bool setThreadName();

private:
    int m_thread_number;
    int m_max_requests;
    pthread_t* m_threads;
    std::list< T* > m_workqueue;
    locker m_queuelocker;
    sem m_queuestat;
    bool m_stop;

    int counter;
    locker m_counterlocker;
};

template< typename T >
threadpool< T >::threadpool( int thread_number, int max_requests ) : 
        m_thread_number( thread_number ), m_max_requests( max_requests ), m_stop( false ), m_threads( NULL ), counter( 0 )
{
    if( ( thread_number <= 0 ) || ( max_requests <= 0 ) )
    {
        throw std::exception();
    }

    m_threads = new pthread_t[ m_thread_number ];
    if( ! m_threads )
    {
        throw std::exception();
    }

    for ( int i = 0; i < thread_number; ++i )
    {
        printf( "create the %dth thread\n", i );
        if( pthread_create( m_threads + i, NULL, worker, this ) != 0 )
        {
            delete [] m_threads;
            throw std::exception();
        }
        if( pthread_detach( m_threads[i] ) )
        {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template< typename T >
threadpool< T >::~threadpool()
{
    delete [] m_threads;
    m_stop = true;
}

template< typename T >
bool threadpool< T >::append( T* request )
{
    m_queuelocker.lock();
    if ( m_workqueue.size() > m_max_requests )
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back( request );
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

/**
 * @brief work thread will execute worker first
 * 
*/
template< typename T >
void* threadpool< T >::worker( void* arg )  
{
    threadpool* pool = ( threadpool* )arg;
    pool->run();
    return pool;
}
/**
 * @brief this method is called by worker, this method is the real funcion that 
 *  thread will execute.
 * 
*/
template< typename T >
void threadpool< T >::run()
{
    setThreadName();
    while ( ! m_stop )
    {
        m_queuestat.wait();
        m_queuelocker.lock();  /**only one thread can entry critical region*/
        if ( m_workqueue.empty() )
        {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();  /** get http_conn */
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if ( ! request )
        {
            continue;
        }
        request->process();  /* begin work by execute process function */
    }
}

template< typename T >
bool threadpool< T >::setThreadName()
{
    static const int maxThreadNameSize = 64;
    /* set thread name. */
    char threadName[maxThreadNameSize];
    m_counterlocker.lock();
    snprintf(threadName, maxThreadNameSize, "%s-%d", "worker", counter++);
    m_counterlocker.unlock();

    pthread_t tid = pthread_self();
    if (pthread_setname_np(tid, threadName) != 0)
    {
        std::cerr << "set thread name: " << threadName <<" failed." << std::endl;
    }

    pthread_getname_np(tid, threadName, maxThreadNameSize);
    std::cout << "tid: " << tid << "tname: " << threadName << std::endl;
}

#endif
