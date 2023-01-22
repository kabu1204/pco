#ifndef PCO_ARCH_H
#define PCO_ARCH_H

#if __cplusplus
extern "C" {
#endif

#include "stdint.h"

#if INTPTR_MAX == INT64_MAX
    #define ARCH64
#elif INTPTR_MAX == INT32_MAX
    #define ARCH32
#endif

#if defined(__i386__) || defined (__x86_64__) || defined(__amd64__) || defined(__arm64__)
#define CPU_CACHE_LINE_SIZE 64
#endif

#if __cplusplus
}
#endif

#endif