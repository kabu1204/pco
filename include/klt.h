#ifndef	_KLT_H
#define _KLT_H

#include "types.h"
#include "atomic.h"
#include <stdatomic.h>

#define KLT_FLAG_SCHED_SHIFT 0
#define KLT_FLAG_SCHED 1 << KLT_FLAG_SCHED_SHIFT

#define klt_sched_flag(k) (k->flag & KLT_FLAG_SCHED)

void test_pthread();

void dumpklt(klt_t* k);

#endif