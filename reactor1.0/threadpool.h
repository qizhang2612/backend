//线程池
#ifndef THREADPOOL
#define THREADPOOL
#include "event_list.h"
#include <pthread.h>

const int THREADPOOL_INVALID = -1;
const int THREADPOOL_LOCK_FAILURE = -2;
const int THREADPOOL_QUEUE_FULL = -3;
const int THREADPOOL_SHUTDOWN = -4;
const int THREADPOOL_THREAD_FAILURE = -5;
const int THREADPOOL_GRACEFUL = 1;

const int MAX_THREADS = 1024;


/**
*  @struct threadpool
*  @brief The threadpool struct
*
*  @var notify       Condition variable to notify worker threads.
*  @var threads      Array containing worker threads ID.
*  @var thread_count Number of threads
*  @var queue        Array containing the task queue.
*  @var queue_size   Size of the task queue.
*  @var head         Index of the first element.
*  @var tail         Index of the next element.
*  @var count        Number of pending tasks
*  @var shutdown     Flag indicating if the pool is shutting down
*  @var started      Number of started threads
*/
//线程池是一种结构体类型
struct threadpool_t{
    //pthread_mutex_t类型，其本质是一个结构体。为简化理解，应用时可忽略其实现细节，简单当成整数看待。
    pthread_mutex_t lock;
    //用于定义条件变量 pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    //线程阻塞的条件变量
    pthread_cond_t notify;
    //线程指针 数组
    pthread_t *threads;
    //线程池任务队列 任务队列中包含回调函数及参数
    std::queue<Event> events;
    int thread_count;//线程池线程数
    //int queue_size;//队列大小
    //int head;//队列的头
    //int tail;//队列的尾部
    //int count;//此时队列的容量 线程任务队列的任务数量
    int shutdown;
    int started;

};

threadpool_t *threadpool_create(int thread_count);
int threadpool_add(threadpool_t *pool,Event &event);
int threadpool_destroy(threadpool_t *pool);
int threadpool_free(threadpool_t *pool);
static void *threadpool_thread(void *threadpool);

#endif
