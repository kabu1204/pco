#ifndef	_KLT_H
#define _KLT_H

#if __cplusplus
extern "C" {
#endif

#include "types.h"
#include "atomic.h"
#include "signal.h"
#include <stdatomic.h>

#define KLT_FLAG_SCHED_SHIFT 0
#define KLT_FLAG_SCHED 1 << KLT_FLAG_SCHED_SHIFT
#define SIG_RESERVED SIGURG

#define klt_sched_flag(k) (k->flag & KLT_FLAG_SCHED)

void test_pthread();

void dumpklt(klt_t* k);

klt_t* klt_create();

#if __cplusplus
}
#endif

#endif