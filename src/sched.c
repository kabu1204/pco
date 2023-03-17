#include "common.h"
#include "scheduler.h"
#include "context/context.h"
#include "klt.h"
#include "atomic.h"
#include "types.h"
#include "ult_queue.h"
#include "util/list.h"
#include "util/scq.h"
#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ucontext.h>
#include <ucontext.h>
#include "log.h"

static lscq_t* global_q = NULL;

void init_globalq(){
    if(global_q == NULL)
        global_q = lscq_create();
}

void ult_put(ult_t* ult){
    push_ult(global_q, ult);
}

void ult_put_localq(ult_t *ult, klt_t *klt){
    struct list_node* node = NULL;
    node = &(container_of(ult, struct ult_list_node, u)->node);
    INFO("addr of ult: %p\n", ult);
    INFO("addr of unode: %p\n", container_of(ult, struct ult_list_node, u));
    INFO("addr of unode->node: %p\n", node);
    list_push_front(&klt->local_list, node);
}

void klt_loop0(klt_t* klt){
    struct list_node* node;
    ult_t* ult;
    printf("in loop0\n");
    while (1) {
        if (pop_ult(global_q, &ult)) {
            ult->bound_klt = 1;
            ult->klt = klt;
            INFO("exec ult %lu\n", ult->id);
            tb_context_jump(ult->stk.sp, NULL);
        }
    }
}

void sched(){
    struct list_node* tmp;
    ult_t* ult;
    klt_t *klt = pthread_getspecific(klt_k);

    printf("enter sched\n");

    if(atomic_bts_u64(&klt->flag, KLT_FLAG_SCHED_SHIFT)){
        return;
    }

    dumpklt(klt);

    // 1. get from local list
    tmp = list_pop_back(&klt->local_list);
    if(tmp) {
        ult = ult_entry(tmp)->u;
        if(klt->ult){
            klt->ult->klt = NULL;
            ult_entry(tmp)->u = klt->ult;
            list_push_front(&klt->local_list, tmp);
        } else {
            free(ult_entry(tmp));
        }
        ult->klt = klt;
        klt->ult = ult;
        goto out;
    }

    // 2. get from global queue
    if(pop_ult(global_q, &ult)){
        if(klt->ult){
            struct ult_list_node* unode = malloc(sizeof(struct ult_list_node));
            klt->ult->klt = NULL;
            unode->u = klt->ult;
            list_push_front(&klt->local_list, &unode->node);
        }
        ult->klt = klt;
        klt->ult = ult;
    }

out:
    atomic_btr_u64(&klt->flag, KLT_FLAG_SCHED_SHIFT);
    printf("leave sched\n");
}