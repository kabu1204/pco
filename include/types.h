#ifndef _PCO_TYPES_H
#define _PCO_TYPES_H

#if __cplusplus
extern "C" {
#endif

#include "sys/syscall.h"
#include "sys/types.h"
#include "signal.h"
#include <bits/types/siginfo_t.h>
#include <stdio.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include "unistd.h"
#include "context/context.h"
#include "arch.h"

static pthread_key_t klt_k;
// struct _klt;
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
    __pid_t bound_klt;
    struct timespec last_sched;
    klt_t* klt;
    struct _ult* next;
    struct _ult_stack stk;
} ult_t;

typedef struct _klt {
    sigevent_t sev;
    timer_t timer_id;
    pthread_t pthread_id;
    __time_t clock_interval;
    struct _ult* ult;
    unsigned long flag;
    
    struct {
        struct _ult* head;
        struct _ult* tail;
    };
} klt_t;

#if __cplusplus
}
#endif

#endif