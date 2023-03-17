#include "context/context.h"
#include "klt.h"
#include "stdio.h"
#include "sigctx.h"
#include "types.h"
#include <time.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/ucontext.h>
#include <time.h>
#include <unistd.h>
#include "ult.h"
#include "scheduler.h"
#include "ult_queue.h"
#include "util/scq.h"
#include "atomic.h"
#include "common.h"
#include "semaphore.h"

struct scq_test_ctl{
    lscq_t* q;
    int start;
    int end;
};

u64 cnt = 0;
u64 cnt2 = 0;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
u64 done = 0;

void* scq_push_fn(void* arg){
    struct scq_test_ctl* ctl = arg;
    int step, end, start, i;
    scq_t* q;
    start = ctl->start;
    end = ctl->end;
    q = ctl->q;
    for(i=start;i<end;++i){
        // atomic_add_u64(&cnt, 1);
        if(!scq_push(q, i)){
            printf("push failed: %d\n", end);
        }
    }
    // printf("push finish: %d %ld\n", end, cnt);
    return NULL;
}
void* scq_pop_fn(void* arg){
    struct scq_test_ctl* ctl = arg;
    int step, end, start, i;
    scq_t* q;
    start = ctl->start;
    end = ctl->end;
    q = ctl->q;
    u64 val;
    for(i=start;i<end;++i){
        while(!scq_pop(q, &val)){
            printf("pop failed: %ld %ld %ld %ld\n", q->head, q->tail, q->threshold, cnt);
        }
        // atomic_add_u64(&cnt2, 1);
    }
    // printf("pop finish: %d %ld\n", end, cnt2);
    return NULL;
}

void* lscq_push_fn(void* arg){
    struct scq_test_ctl* ctl = arg;
    int step, end, start, i;
    lscq_t* q;
    start = ctl->start;
    end = ctl->end;
    q = ctl->q;
    for(i=start;i<end;++i){
        // atomic_add_u64(&cnt, 1);
        lscq_push(q, i);
    }
    // printf("push finish: %d %ld\n", end, cnt);
    atomic_add_u64(&done, 1);
    pthread_cond_signal(&cond);
    return NULL;
}
void* lscq_pop_fn(void* arg){
    struct scq_test_ctl* ctl = arg;
    int step, end, start, i;
    lscq_t* q;
    start = ctl->start;
    end = ctl->end;
    q = ctl->q;
    u64 val;
    for(i=start;i<end;++i){
        while(!lscq_pop(q, &val));
        // atomic_add_u64(&cnt2, 1);
    }
    atomic_add_u64(&done, 1);
    pthread_cond_signal(&cond);
    // printf("pop finish: %d %ld\n", end, cnt2);
    return NULL;
}

void scq_test(){
    int i;
    int n_thread = 32;
    int per_thread = 65536;
    int total_ops = n_thread*per_thread;
    struct timespec start_time, end_time;
    double cost;

    cnt = 0;
    // pthread_t t[32];
    pthread_t* t = malloc(n_thread*sizeof(pthread_t));
    struct scq_test_ctl* ctls = malloc(n_thread*sizeof(struct scq_test_ctl));
    lscq_t* q = lscq_create();
    for(i=0;i<n_thread;i+=2){
        ctls[i+1].q = ctls[i].q = q;
        ctls[i+1].start = ctls[i].start = (i/2)*per_thread;
        ctls[i+1].end = ctls[i].end = (i/2+1)*per_thread;
        // printf("i:  %d, %d\n", ctls[i].start, ctls[i].end);
        // printf("i+1:%d, %d\n", ctls[i+1].start, ctls[i+1].end);
    }
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for(i=0;i<n_thread;i+=2){
        pthread_create(&t[i], NULL, lscq_push_fn, &ctls[i]);
        // pthread_create(&t[i+1], NULL, lscq_pop_fn, &ctls[i+1]);
    }
    for(i=0;i<n_thread;i+=2){
        pthread_create(&t[i+1], NULL, lscq_pop_fn, &ctls[i+1]);
    }
    while(atomic_load_u64(&done)<n_thread){
        pthread_cond_wait(&cond, &lk);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    cost = (end_time.tv_nsec - start_time.tv_nsec) + (end_time.tv_sec - start_time.tv_sec)*1e9;
    printf("total_ops=%d, cnt=%ld, %.2f ns/op\n", total_ops, cnt, cost/(double)total_ops);
    for(i=0;i<n_thread;i+=2){
        pthread_join(t[i], NULL);
        pthread_join(t[i+1], NULL);
    }
    lscq_destroy(q);
    free(t);
    free(ctls);
    // return NULL;
}

void* secondary(void *arg){
    double a = 10.12;
    printf("secondary %.2f\n", a);
}

void* do_something(void *arg){
    struct scq_test_ctl* ctl;
    ctl = arg;
    printf("working well\n");
    printf("%d, %d\n", ctl->start, ctl->end);
    scq_test();
    return NULL;
}

void test_ult_entry(){
    struct scq_test_ctl* arg;
    struct ult_list_node unode;
    ult_t *ult;
    klt_t *klt;
    printf("test\n");
    arg = malloc(sizeof(struct scq_test_ctl));
    arg->start = 1234;
    arg->end   = 5678;
    ult = ult_create(8192, do_something, arg);
    klt = klt_create();
    unode.u = ult;
    ult_put(ult);
    while(1);
    // tb_context_jump(ult->stk.sp, NULL);
}

int main(){
    // scq_test();
    // test_pthread();
    // char* stk = malloc(8192);
    // tb_context_t sp = tb_context_make(stk, 8192, scq_test);
    // tb_context_jump(sp, NULL);
    test_ult_entry();
    return 0;
}