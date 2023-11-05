#ifndef REACTOR
#define REACTOR

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

class reactor
{
private:
    /* data */
    int epfd;
    
public:
    reactor(/* args */);
    ~reactor();
};

reactor::reactor(/* args */)
{
}

reactor::~reactor()
{
}





#endif