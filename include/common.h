#ifndef _PCO_COMMON_H
#define _PCO_COMMON_H

#if __cplusplus
extern "C" {
#endif

#include <sys/types.h>
typedef unsigned long u64;
typedef unsigned char u8;
typedef long long i64;
typedef void* (*ult_func_t)(void*);
typedef struct {
    u64 flag, data;
} u128;

#define likely(exp) __builtin_expect(!!(exp), 1)
#define unlikely(exp) __builtin_expect(!!(exp), 0)

#if __cplusplus
}
#endif

#endif