#ifndef UTILS
#define UTILS
#include <unistd.h>
#include <pthread.h>
#include <deque>
#include <string>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>

using Handler_t = int (*)(int fd,int events,void *arg);


//-打印线程错误专用，根据err来识别错误信息
static inline void ERR_EXIT_THREAD(int err, const char * msg){
    fprintf(stderr,"%s:%s\n",strerror(err),msg);
    exit(EXIT_FAILURE);
}
#endif