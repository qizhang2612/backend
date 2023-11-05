#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include "buffer.h"

#define min(lth,rth) ((lth)<(rth)?(lth):(rth))

//将ringbuffer size 调整成2的幂，优化取余
static inline int is_power_of_two(uint32_t num) {
    if (num < 2) return 0;
    return (num & (num - 1)) == 0;
}

static inline uint32_t roundup_power_of_two(uint32_t num) {
    if (num == 0) return 2;
    int i = 0;
    for (; num != 0; i++)
    {
        num >>= 1;
    }
    return 1U << i;
    
}

void buffer_init(buffer_t *r,uint32_t sz){
    if (!is_power_of_two(sz)) sz = roundup_power_of_two(sz);
    r->buf = (char *)malloc(sz * sizeof(char));
    r->size = sz;
    r->tail = 0;
    r->head = 0;
    
}

void buffer_free(buffer_t *r){
    if(r->buf != 0){
        free(r->buf);
        r->buf = 0;
    }
    r->head = r->tail = r->size = 0;
}

static uint32_t rb_isempty(buffer_t *r){
    return r->head == r->tail;
}

static uint32_t rb_isfull(buffer_t *r){
    return r->size == (r->tail - r->head);
}

static uint32_t rb_len(buffer_t *r){
    return r->tail - r->head;
}

static uint32_t rb_remain(buffer_t *r){
    return r->size - r->tail + r->head;
}

int buffer_add(buffer_t *r,const void *data,uint32_t sz) {
    if(sz > rb_remain(r)){
        return -1;
    }

#ifdef USE_MB
//确保开始移动buffer的时候 read_pos
    smp_mb();
#endif

    uint32_t i;
    i = min(sz,r->size - (r->tail &(r->size - 1)));

    memcpy(r->buf + (r->tail &(r->size - 1)),data,i);
    memcpy(r->buf,data + i,sz - i);

#ifdef USE_MB
//确保write_pos 不会优化到前面
    smp_wmb();
#endif

    r->tail += sz;
    return 0;
}

int buffer_remove(buffer_t *r, void *data, uint32_t sz){
    assert(!rb_isempty(r));//r为空则报错
    uint32_t i;
    sz = min(sz,r->tail - r->head);

#ifdef USE_MB
//确保read_pos 可见性
    smp_wmb();
#endif

    i = min(sz,r->size - (r->head & (r->size - 1)));
    memcpy(data,r->buf + (r->head & (r->size - 1)),i);
    memcpy(data+i, r->buf, sz-i);

#ifdef USE_MB
//确保read_pos 不会优化到前面
    smp_mb();
#endif

    r->head += sz;
    return sz;
}

int buffer_drain(buffer_t *r, uint32_t sz){
    if(sz > rb_len(r)) sz = rb_len(r);
    r->head += sz;
    return sz;
}

// 找 buffer 中 是否包含特殊字符串（界定数据包的）
int buffer_search(buffer_t *r, const char* sep, const int seplen){
    int i;
    for (i = 0; i < rb_len(r) - seplen; i++){
        int pos = (r->head + i) & (r->size - 1);
        if (pos + seplen > r->size){
            if (memcmp(r->buf + pos,sep,r->size - pos)) return 0;
            if (memcmp(r->buf, sep, pos + seplen - r->size) == 0) return i + seplen;  
        }
        if (memcmp(r->buf + pos,sep,seplen) == 0) return i + seplen;   
    }
    return 0;
}

uint32_t buffer_len(buffer_t *r) {
    return rb_len(r);
}

uint8_t * buffer_write_atmost(buffer_t *r) {
    //写到fd里的
    uint32_t rpos = r->head & (r->size - 1);
    uint32_t wpos = r->tail & (r->size - 1);
    if(wpos < rpos) {
        char* temp = (char *)malloc(r->size * sizeof(char));
        memcpy(temp,r->buf+rpos,r->size - rpos);
        memcpy(temp+r->size-rpos,r->buf,wpos);
        free(r->buf);
        r->buf = temp;
        return r->buf;
    }
    return r->buf + rpos;
}

