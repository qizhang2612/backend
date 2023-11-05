#ifndef _chain_buffer_h
#define _chain_buffer_h
#include <stdint.h>

typedef struct buf_chain_s {
    struct buf_chain_s *next;
    uint32_t buffer_len;
    uint32_t misalign;//避免数据移动，记录位置
    uint32_t off;//实际存储数据的长的
    uint8_t *buffer;
} buf_chain_t;

typedef struct buffer_s {
    buf_chain_t *first;
    buf_chain_t *last;
    buf_chain_t **last_with_datap;
    uint32_t total_len;
    uint32_t last_read_pos; // for sep read
} buffer_t;

uint32_t buffer_len(buffer_t *buf);

void buffer_init(buffer_t *buf, uint32_t sz);

int buffer_add(buffer_t *buf, const void *data, uint32_t datlen);

int buffer_remove(buffer_t *buf, void *data, uint32_t datlen);

int buffer_drain(buffer_t *buf, uint32_t len);

void buffer_free(buffer_t *buf);

int buffer_search(buffer_t *buf, const char* sep, const int seplen);

uint8_t * buffer_write_atmost(buffer_t *p);

#endif
