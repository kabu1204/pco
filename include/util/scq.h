#ifndef	PCO_SCQ_H
#define PCO_SCQ_H

#include "arch.h"
#include "common.h"
#include "atomic.h"
#include "stdlib.h"
#include <stdlib.h>

#define SCQ_SHIFT 16
#define SCQ_SIZE (1ull<<SCQ_SHIFT)
#define SCQ_BUF_SIZE (SCQ_SIZE<<1)
#define SCQ_BUF_SIZE_MASK (SCQ_BUF_SIZE-1)
#define SCQ_TAIL_CYCLE_MASK (~SCQ_BUF_SIZE_MASK)
#define SCQ_CLOSED (1ull<<63)
#define SCQ_TAIL_MASK (~SCQ_CLOSED)
#define SCQ_FLAG_UNUSED (1ull<<63)
#define SCQ_FLAG_SAFE (1ull<<62)
#define SCQ_FLAG_MASK (0b11ull<<62)
#define SCQ_CYCLE_MASK (~SCQ_FLAG_MASK)
#define SCQ_RESET_THRESHOLD (SCQ_BUF_SIZE + 1)

#define cache_remap_8B(idx) (((idx & SCQ_BUF_SIZE_MASK) >> (SCQ_SHIFT - 2)) | ((idx << 3) & SCQ_BUF_SIZE_MASK))

typedef struct scq {
    u64 head;
    u8 _[CPU_CACHE_LINE_SIZE-sizeof(u64)];
    u64 tail;
    u8 __[CPU_CACHE_LINE_SIZE-sizeof(u64)];
    i64 threshold;
    u8 ___[CPU_CACHE_LINE_SIZE-sizeof(i64)];
    struct scq* next;
    u128 buf[SCQ_BUF_SIZE+1];  // the extra 1 u128 is reserved for 16bytes alignment
    u128* entries;
} scq_t;

size_t align16_backward(size_t x);

scq_t* scq_create();

void scq_free(scq_t* q);

int scq_push(scq_t* q, u64 val);

int scq_pop(scq_t* q, u64* val);

void scq_catchup(scq_t* q, u64 tail, u64 head);

typedef struct lscq {
    scq_t* head;
    u8 _[CPU_CACHE_LINE_SIZE-sizeof(u64)];
    scq_t* tail;
} lscq_t;

void lscq_push(scq_t* q, u64 val);

int lscq_pop(scq_t* q, u64* val);

#endif