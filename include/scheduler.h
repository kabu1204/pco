#ifndef _SCHED_H
#define _SCHED_H

#if __cplusplus
extern "C" {
#endif

#include "types.h"
#include "ult.h"
#include "klt.h"

void init_globalq();

void ult_put_localq(ult_t* ult, klt_t* klt);

void ult_put(ult_t* ult);

void klt_loop0(klt_t* klt);

void sched();

#if __cplusplus
}
#endif

#endif