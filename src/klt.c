#include "pthread.h"
#include "sys/syscall.h"
#include "sys/types.h"
#include "consts.h"
#include "context/context.h"
#include "sigctx.h"
#include "signal.h"
#include "pthread.h"
#include <stdio.h>
#include <sys/ucontext.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include "types.h"
#include "unistd.h"
#include "klt.h"
#include "string.h"
#include "ult.h"
#include "atomic.h"

static ult_t* ult_pool[2];

static size_t cpy_long(long x, char *buf, size_t start){
    size_t size = 0;
    if(x==0) {
        buf[start++] = '0';
        size++;
    }
    while(x){
        buf[start++] = '0' + x % 10;
        x /= 10;
        size++;
    }
    buf[start] = '\0';
    return size;
}

// dumpklt is NOT async-safe.
void dumpklt(klt_t* k) {
    if(k == NULL) return;

    printf("----DUMP KLT----\n");
    printf("Clock interval: %ld\n", k->clock_internal);
    printf("Pthread id:     %ld\n", k->pthread_id);
    printf("Timer id:       %p\n", k->timer_id);
    printf("Running ULT:    %s\n", (k->ult==NULL)?"false":"true");
    printf("ULT id:         %ld\n", (k->ult==NULL)?-1:(k->ult->id));
    printf("Flag:           %ld\n", k->flag);
}

static void init_sigevent(sigevent_t* sev, timer_t* timer_id)
{
    sev->sigev_notify = SIGEV_THREAD_ID;
    sev->_sigev_un._tid = syscall(SYS_gettid);
    sev->sigev_signo = SIGUSR1;
    if(timer_create(CLOCK_MONOTONIC, sev, timer_id) == -1) {
        fprintf(stderr, "failed to create timer");
    }
}

static void init_timer(timer_t timer_id, __time_t internal_ns)
{
    struct itimerspec its;
    const __time_t ns_per_s = 1e9;
    its.it_value.tv_sec = internal_ns / ns_per_s;
    its.it_value.tv_nsec = internal_ns % ns_per_s;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    if(timer_settime(timer_id, 0, &its, NULL) == -1){
        fprintf(stderr, "failed to start timer");
    }
}

static void* init_fn(void *this_klt)
{
    timer_t* timer_id_p = &((klt_t*)this_klt)->timer_id;
    sigevent_t* sev_p = &((klt_t*)this_klt)->sev;
    ult_t* init_ult = ult_pool[0];
    tb_context_from_t from;

    printf("init_fn called\n");

    pthread_setspecific(klt_k, this_klt);
    init_sigevent(sev_p, timer_id_p);
    init_timer(*timer_id_p, ((klt_t*)this_klt)->clock_internal);

    printf("init_timer end\n");

    init_ult->klt = this_klt;
    ((klt_t*)this_klt)->ult = init_ult;
    ((klt_t*)this_klt)->pthread_id = 123;

    printf("this_klt = %p\n", this_klt);
    printf("thread_local = %p\n", pthread_getspecific(klt_k));

    // from = tb_context_jump(init_ult->stk.sp, NULL);
    // printf("jump back\n");
    while(1);
}

void sa_sighandler(int signum, siginfo_t *info, void *ctx) {
    sigctxt_t sigctx = {
        .ctxt = (ucontext_t*)ctx,
        .info = info,
    };

    inject_call0(&sigctx, preempt_call);

    dumpgregs(&sigctx);
    write(1, "leaving sighandler\n", 19);
}

void test_pthread()
{

    unsigned long a = 10;
    printf("atomic_load(a): %ld\n", atomic_load_u64(&a));
    atomic_store_u64(&a, 123);
    printf("atomic_load(a): %ld\n", atomic_load_u64(&a));
    printf("cas: %d\n", atomic_cas_u64(&a, 10, 123));
    printf("atomic_load(a): %ld\n", atomic_load_u64(&a));
    printf("cas: %d\n", atomic_cas_u64(&a, 123, 456));
    printf("atomic_load(a): %ld\n", atomic_load_u64(&a));
    a = 0;
    printf("bts: %d\n", atomic_bts_u64(&a, 4));
    printf("a:  %ld\n", a);
    printf("bts: %d\n", atomic_bts_u64(&a, 4));
    printf("a:  %ld\n", a);
    printf("bts: %d\n", atomic_bts_u64(&a, 0));
    printf("a:  %ld\n", a);
    printf("btr: %d\n", atomic_btr_u64(&a, 4));
    printf("a:  %ld\n", a);
    printf("btr: %d\n", atomic_btr_u64(&a, 4));
    printf("a:  %ld\n", a);

    struct timespec ts;
    ult_pool[0] = ult_create(STACK_SIZE);
    ult_pool[1] = ult_create(STACK_SIZE);

    printf("ult0->id: %ld\n", ult_pool[0]->id);
    printf("ult1->id: %ld\n", ult_pool[1]->id);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    printf("tv_sev: %ld\t tv_ns: %ld\n", ts.tv_sec, ts.tv_nsec);
    pthread_t t1, t2;
    klt_t klt1, klt2;
    klt1.clock_internal = 1e9;
    klt2.clock_internal = 3e9;
    klt1.flag = 0;
    // signal(SIGUSR1, sigusr1_handler);

    struct sigaction sa;
    sa.sa_sigaction = sa_sighandler;

    sigaction(SIGUSR1, &sa, NULL);
    pthread_key_create(&klt_k, NULL);
    pthread_create(&t1, NULL, init_fn, &klt1);
    // pthread_create(&t2, NULL, init_fn, &klt2);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    printf("tv_sev: %ld\t tv_ns: %ld\n", ts.tv_sec, ts.tv_nsec);

    pthread_join(t1, NULL);
    // pthread_join(t2, NULL);
}