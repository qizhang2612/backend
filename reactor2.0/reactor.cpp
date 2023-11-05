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
#include "threadpool.h"
#include "timer.h"
#include "utils.h"

#define BUFFER_LENGTH           4096
#define MAX_EPOLL_EVENTS        1024
#define SERVER_PORT             8082

struct zqevent{//文件描述符及其对应事件
    int fd;
    int events;
    void *arg;//reactor

    Handler_t  callback;

    int status;//1 mod 0 add
    char buffer[BUFFER_LENGTH];
    int length;
    long last_active;
};

struct zqreactor{
    int epfd;//红黑树
    struct zqevent *events; //事件队列
};

int recv_cb(int fd,int events,void *arg);
int send_cb(int fd,int events,void *arg);
int accept_cb(int fd,int events,void *arg);

void zq_event_set(struct zqevent *ev,int fd,Handler_t callback,void *arg){
    ev->fd = fd;
    ev->callback = callback;
    ev->events = 0;
    ev->arg = arg;
    ev->last_active = time(NULL);
}

int zq_event_add(int epfd,int events,struct zqevent *zqev){
    struct epoll_event ev = {0,{0}};
    ev.data.ptr = zqev;
    ev.events = zqev->events = events;
    int op;
    if(zqev->status == 1){
        op = EPOLL_CTL_MOD;
    }else{
        op = EPOLL_CTL_ADD;
        zqev->status = 1;
    }
    if(epoll_ctl(epfd,op,zqev->fd,&ev) < 0){
        printf("event add failed [fd=%d], events[%d],err:%s,err:%d\n", zqev->fd, events,strerror(errno),errno);
        return -1;
    }
    return 0;
}

int zq_event_del(int epfd,struct zqevent *zqev){
    struct epoll_event ev = {0,{0}};
    if(zqev->status != 1){
        return -1;
    }
    ev.data.ptr = zqev;
    zqev->status = 0;
    epoll_ctl(epfd,EPOLL_CTL_DEL,zqev->fd,&ev);
    return 0;
}

int init_sock(short port){
    int fd = socket(AF_INET,SOCK_STREAM,0);
    fcntl(fd,F_SETFL,O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(fd,(struct sockaddr *)&server_addr,sizeof(server_addr));
    if(listen(fd,20) < 0){
        printf("listen failed : %s\n", strerror(errno));
    }
    return fd;
}

int accept_cb(int fd,int events,void *arg){
    struct zqreactor *reactor = (struct zqreactor *)arg;
    if(reactor == NULL) return -1;
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int clientfd;

    if(clientfd = accept(fd,(struct sockaddr*)&client_addr,&len) == -1){
        printf("accept: %s\n", strerror(errno));
        return -1;        
    }
    printf("client fd = %d\n",clientfd);
    int flag = 0;
    if(flag = fcntl(clientfd,F_SETFL,O_NONBLOCK) < 0){
        printf("%s: fcntl nonblocking failed, %d\n", __func__, MAX_EPOLL_EVENTS);
        return -1;        
    }
    zq_event_set(&reactor->events[clientfd],clientfd,recv_cb,reactor);
    zq_event_add(reactor->epfd,EPOLLIN,&reactor->events[clientfd]);

    printf("new connect [%s:%d][time:%ld], pos[%d]\n",inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), reactor->events[clientfd].last_active, clientfd);
    return 0;
}

int recv_cb(int fd,int events,void *arg){
    struct zqreactor *reactor = (struct zqreactor *)arg;
    struct zqevent *zqev = &reactor->events[fd];
    int len = recv(fd,zqev->buffer,BUFFER_LENGTH,0);
    zq_event_del(reactor->epfd,zqev);
    if(len > 0){
        zqev->length = len;
        zqev->buffer[len] = '\0';
        printf("C[%d]:%s\n", fd, zqev->buffer);
        zq_event_set(zqev,fd,send_cb,reactor);
        zq_event_add(reactor->epfd,EPOLLOUT,zqev);
    }else if(len == 0){
        close(zqev->fd);
        printf("[fd=%d] pos[%ld], closed\n", fd, zqev - reactor->events);
    }else{
        close(zqev->fd);
        printf("recv[fd=%d] error[%d]:%s\n", fd, errno, strerror(errno));
    }
    return len;
}

int send_cb(int fd,int events,void *arg){
    struct zqreactor *reactor = (struct zqreactor *)arg;
    struct zqevent *zqev = &reactor->events[fd];
    int len = send(fd,zqev->buffer,zqev->length,0);
    if(len > 0){
        printf("send[fd=%d], [%d]%s\n", fd, len, zqev->buffer);
        zq_event_del(reactor->epfd,zqev);
        zq_event_set(zqev,fd,recv_cb,reactor);
        zq_event_add(reactor->epfd,EPOLLIN,zqev);
    }else{
        close(zqev->fd);
        zq_event_del(reactor->epfd, zqev);
        printf("send[fd=%d] error %s\n", fd, strerror(errno));
    }
    return len;
}

int zqreactor_init(struct zqreactor *reactor){
    if(reactor == NULL) return -1;
    memset(reactor,0,sizeof(struct zqreactor *));
    reactor->epfd = epoll_create(1);
    printf("ep fd=%d\n",reactor->epfd);
    if (reactor->epfd <= 0) {
        printf("create epfd in %s err %s\n", __func__, strerror(errno));
        return -2;
    }
    reactor->events = (struct zqevent *)malloc(sizeof(struct zqevent)*MAX_EPOLL_EVENTS);
    memset(reactor->events, 0, (MAX_EPOLL_EVENTS) * sizeof(struct zqevent));
    if (reactor->events == NULL) {
        printf("create epll events in %s err %s\n", __func__, strerror(errno));
        close(reactor->epfd);
        return -3;
    }
    return 0;    
}

int zqreactor_destory(struct zqreactor *reactor){
    close(reactor->epfd);
    free(reactor->events);
}

int zqreactor_addlistener(struct zqreactor *reactor,int sockfd,Handler_t acceptor){
    if(reactor == NULL) return -1;
    if(reactor->events == NULL) return -1;
    printf("listen sockfd=%d\n", sockfd);
    zq_event_set(&reactor->events[sockfd],sockfd,acceptor,reactor);
    zq_event_add(reactor->epfd,EPOLLIN,&reactor->events[sockfd]);
    return 0;
}

int zqreactor_run(struct zqreactor *reactor,std::shared_ptr<ThreadPool> pool,std::shared_ptr<Timer> timer) {
    if (reactor == NULL) return -1;
    if (reactor->epfd < 0) return -1;
    if (reactor->events == NULL) return -1;

    struct epoll_event events[MAX_EPOLL_EVENTS];
    int checkpos = 0,i;
    while(1){
        //心跳包 60s 超时则断开连接（保活）
        long now = time(NULL);
        for(i = 0;i < 100;i++,checkpos++){
            if (checkpos == MAX_EPOLL_EVENTS) {
                checkpos = 0;
            }
            if (reactor->events[checkpos].status != 1 || checkpos == 3) {
                continue;
            }
            long duration = now - reactor->events[checkpos].last_active;
            if (duration > 60){
                close(reactor->events[checkpos].fd);
                printf("[fd=%d] timeout\n", reactor->events[checkpos].fd);
                zq_event_del(reactor->epfd,&reactor->events[checkpos]);
            }
        }
        /*
        1、当时间堆中没有定时器时，epoll_wait的超时时间T设为-1，表示一直阻塞等待新用户的到来；
        2、当时间堆中有定时器时，epoll_wait的超时时间T设为最小堆堆顶的超时值，
        这样可以保证让最近触发的定时器能得以执行；
        3、在epoll_wait阻塞等待期间，若有其它的用户到来，则epoll_wait返回n>0，
        进行常规的处理，随后应重新设置epoll_wait为小顶堆堆顶的超时时间。*/
        while(timer->CheckTimer()){
            int nready = epoll_wait(reactor->epfd,events,MAX_EPOLL_EVENTS,timer->TimeToSleep());
            if (nready < 0) {
                printf("epoll_wait error, exit\n");
                continue;
            }
            for(i = 0;i < nready;i++){
                struct zqevent *zqev = (struct zqevent *)events[i].data.ptr;
                if((zqev->events & EPOLLIN) && (events[i].events & EPOLLIN)){
                    pool->push_task(i,zqev->events,zqev->arg,zqev->callback);
                    //zqev->callback(zqev->fd,zqev->events,zqev->arg);
                }
                if((zqev->events & EPOLLOUT) && (events[i].events & EPOLLOUT)){
                    pool->push_task(i,zqev->events,zqev->arg,zqev->callback);
                //zqev->callback(zqev->fd,zqev->events,zqev->arg);
                }
            }
        }     
    }
}

int main(int argc, char *argv[]) {
    int i = 0;
    int sockfd = init_sock(SERVER_PORT);
    std::shared_ptr<ThreadPool> pool(new ThreadPool(100));
    pool->createPool();
    std::shared_ptr<Timer> timer(new Timer());
    timer->AddTimer(1000,[&](const TimerNode &node){
         cout << Timer::GetTick() << "node id" << node.id << "revoked times:" << ++i << endl;
    });
    struct zqreactor * reactor = (struct zqreactor *)malloc(sizeof(struct zqreactor));
    if(zqreactor_init(reactor) != 0){
        return -1;
    }
    zqreactor_addlistener(reactor,sockfd,accept_cb);
    zqreactor_run(reactor,pool,timer);
    zqreactor_destory(reactor);
    close(sockfd);
    return 0;
}
