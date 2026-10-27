#ifndef _PCO_TYPES_H
#define _PCO_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include "common.h"
#include "context/context.h"
#include "arch.h"
#include "util/list.h"

typedef struct _klt klt_t;

typedef struct _sigctxt {
    siginfo_t* info;
    void* ctxt;
} sigctxt_t;

typedef struct _ult_stack {
    char *space;
    tb_context_t sp;
} ult_stack_t;

typedef struct _ult {
    unsigned long id;
    pid_t bound_klt_id;
    int bound_klt;
    struct timespec last_sched;
    klt_t* klt;
    ult_stack_t stk;
    ult_func_t entry;
    void *arg;
    struct list_node queue_node;
    int finished;
    int queue_state;
    int parking;
    tb_context_t sched_ctx;
    struct pco_executor *executor;
    struct pco_ult_local_state *local_state;
} ult_t;

typedef struct _klt {
    struct sigevent sev;
    timer_t timer_id;
    pthread_t pthread_id;
    time_t clock_interval;
    ult_t* ult;
    unsigned long flag;
    struct list_node local_list;
    tb_context_t sched_ctx;
    int stopping;
    int prefer_global;
    struct pco_worker_runtime *runtime;
} klt_t;

#ifdef __cplusplus
}
#endif
#endif
