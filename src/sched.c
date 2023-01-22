#include "scheduler.h"
#include "context/context.h"
#include "klt.h"
#include "atomic.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/ucontext.h>
#include <ucontext.h>

void sched(){
    printf("enter sched\n");
    klt_t *klt = pthread_getspecific(klt_k);

    if(atomic_bts_u64(&klt->flag, KLT_FLAG_SCHED_SHIFT)){
        return;
    }

    dumpklt(klt);

    atomic_btr_u64(&klt->flag, KLT_FLAG_SCHED_SHIFT);
    printf("leave sched\n");
}