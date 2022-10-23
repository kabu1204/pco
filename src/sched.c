#include "sched.h"
#include "context/context.h"
#include "klt.h"
#include "atomic.h"
#include <pthread.h>
#include <stdatomic.h>
#include <sys/ucontext.h>
#include <ucontext.h>

void sched(){
    printf("enter sched\n");
    klt_t *klt = pthread_getspecific(klt_k);

    printf("rflags: %ld\n", read_rflags());
    unsigned long b = 0;
    int a = atomic_bts_u64(&b, 0);
    printf("a: %d\n", a);
    printf("b: %ld\n",b);

    if(atomic_bts_u64(&klt->flag, KLT_FLAG_SCHED)){
        return;
    }

    atomic_btr_u64(&klt->flag, KLT_FLAG_SCHED);
    printf("leave sched\n");
}