#ifndef _EVENT_LIST_H_
#define _EVENT_LIST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <cstdint>
#include <queue>
#include <memory>
#include <pthread.h>


class Event{
private:
    using Handler_t = int (*)(int fd,int events,void *arg);
    int fd;
    int events;
    void *arg;
    Handler_t handler;

public:
    Event() = default;
    Event(int fd,int events,void *arg, Handler_t handler){
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


class EventList{
private:
    pthread_spinlock_t lock;
    std::queue<Event> events;

public:
    EventList()
    {
        pthread_spin_init(&lock, PTHREAD_PROCESS_SHARED);
    }
    bool Empty()
    {
        return events.empty();
    }
    void ExtractAndHandle();
    void AddEvent(Event &event);
};

#endif // _EVENT_LIST_H_