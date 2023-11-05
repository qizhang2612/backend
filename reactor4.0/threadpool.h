#ifndef THREADPOOL
#define THREADPOOL
#include <unistd.h>
#include <pthread.h>
#include <deque>
#include <string>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//- 任务队列元素
class TaskEle{
private:
    int fd;
    int events;
    void *arg;
    Handler_t handler;

public:
    TaskEle() = default;
    TaskEle(int fd,int events,void *arg, Handler_t handler){
        this->fd = fd;
        this->events = events;
        this->arg = arg;
        this->handler = handler;
    }

    void HandleEvent()
    {
        handler(this->fd,this->events,this->arg);
    }
};

//-执行队列元素
class ExecEle{
public:
    pthread_t tid;
    bool usable = true;
    ThreadPool* pool;
    static void* start(void* arg);
};

//-线程池
class ThreadPool{
public:
    //-任务队列和执行队列
    std::deque<TaskEle*> task_queue;
    std::deque<ExecEle*> exec_queue;
    //-条件变量
    pthread_cond_t cont;
    //-互斥锁
    pthread_mutex_t mutex;
    //-线程池大小
    int thread_count;
    //-构造函数
    ThreadPool(int thread_count):thread_count(thread_count){
        //-初始化条件变量和互斥锁
        pthread_cond_init(&cont,NULL);
        pthread_mutex_init(&mutex,NULL);
    }
    void createPool(){
        int ret;
        //-初始执行队列
        for(int i = 0;i<thread_count;++i){
            ExecEle *ee = new ExecEle;
            ee->pool = const_cast<ThreadPool*>(this);
            if(ret = pthread_create(&(ee->tid),NULL,ee->start,ee)){
                delete ee;
                ERR_EXIT_THREAD(ret,"pthread_create");
            }
            fprintf(stdout,"create thread %d\n",i);
            exec_queue.push_back(ee);
        }
        fprintf(stdout,"create pool finish...\n");
    }
    //-加入任务
    void push_task(int fd,int events,void *arg, Handler_t handler){
        TaskEle *te = new TaskEle(fd,events,arg,handler);
        //-加锁
        pthread_mutex_lock(&mutex);
        task_queue.push_back(te);
        //-通知执行队列中的一个进行任务
        pthread_cond_signal(&cont);
        //-解锁
        pthread_mutex_unlock(&mutex);

    }
    //-销毁线程池
    ~ThreadPool() {
        for(int i = 0;i<exec_queue.size();++i){
            exec_queue[i]->usable = false;
        }
        pthread_mutex_lock(&mutex);
        //-清空任务队列
        task_queue.clear();
        //-广播给每个执行线程令其退出(执行线程破开循环会free掉堆内存)
        pthread_cond_broadcast(&cont);
        pthread_mutex_unlock(&mutex);//-让其他线程拿到锁
        //-等待所有线程退出
        for(int i = 0;i<exec_queue.size(); ++i){
            pthread_join(exec_queue[i] -> tid,NULL);
        }
        //-清空执行队列
        exec_queue.clear();
        //-销毁锁和条件变量
        pthread_cond_destroy(&cont);
        pthread_mutex_destroy(&mutex);
    }
};

void* ExecEle::start(void*arg){
    //-获得执行对象
    ExecEle *ee = (ExecEle*)arg;
    while(true){
        //-加锁
        pthread_mutex_lock(&(ee->pool->mutex));
        while(ee->pool->task_queue.empty()){//-如果任务队列为空，等待新任务
            if(!ee->usable){
                break;
            }
            pthread_cond_wait(&ee->pool->cont, &ee->pool->mutex);
        }
        if(!ee -> usable){
            pthread_mutex_unlock(&ee -> pool -> mutex);
            break;
        }
        TaskEle *te = ee->pool->task_queue.front();
        ee->pool->task_queue.pop_front();
        //-解锁
        pthread_mutex_unlock(&(ee->pool -> mutex));
        //-执行任务回调
        te->HandleEvent();
    }
    //-删除线程执行对象
    delete ee;
    fprintf(stdout,"destroy thread %d\n",pthread_self());
    return NULL;
}
#endif