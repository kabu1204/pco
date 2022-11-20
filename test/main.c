#include "klt.h"
#include "stdio.h"
#include "sigctx.h"
#include "types.h"
#include <pthread.h>
#include <stddef.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include "util/scq.h"
#include "atomic.h"
#include "common.h"

struct scq_test_ctl{
    scq_t* q;
    int start;
    int end;
};

u64 cnt;
u64 cnt2;

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
        atomic_add_u64(&cnt2, 1);
    }
    printf("pop finish: %d %ld\n", end, cnt2);
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
        atomic_add_u64(&cnt, 1);
        lscq_push(q, i);
    }
    printf("push finish: %d %ld\n", end, cnt);
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
        while(!lscq_pop(q, &val)){
            printf("pop failed: %ld\n", cnt2);
        }
        atomic_add_u64(&cnt2, 1);
    }
    printf("pop finish: %d %ld\n", end, cnt2);
    return NULL;
}

void scq_test(){
    int i;
    int per_thread = 65536*10/16;
    cnt = 0;
    pthread_t t[32];
    struct scq_test_ctl ctls[32];
    lscq_t* q = lscq_create();
    for(i=0;i<32;i+=2){
        ctls[i+1].q = ctls[i].q = q;
        ctls[i+1].start = ctls[i].start = (i/2)*per_thread;
        ctls[i+1].start = ctls[i].end = (i/2+1)*per_thread;
        printf("i:  %d, %d\n", ctls[i].start, ctls[i].end);
        printf("i+1:%d, %d\n", ctls[i+1].start, ctls[i+1].end);
    }
    for(i=0;i<32;i+=2){
        pthread_create(&t[i], NULL, lscq_push_fn, &ctls[i]);
        pthread_create(&t[i+1], NULL, lscq_pop_fn, &ctls[i+1]);
    }
    for(i=0;i<32;i+=2){
        pthread_join(t[i], NULL);
        pthread_join(t[i+1], NULL);
    }
    printf("cnt=%ld\n", cnt);
}

int main(){
    scq_test();

    return 1;

    printf("align of scq: %ld\n", _Alignof(scq_t));
    scq_t* x = scq_create();
    printf("addr of scq: %p\n", x);
    printf("addr of scq->buf: %p\n", x->buf);
    printf("addr of scq->entries: %p\n", x->entries);
    
    
    u128 n = {11, 22};
    u128 xx, xxx;
    int res;
    xxx.flag = 123; xxx.data = 456;
    xx = atomic_load_u128(&n);
    printf("atomic_load_u128: %ld, %ld\n", xx.flag, xx.data);
    xx.flag = n.data;
    xx.data = n.flag;
    res = atomic_cas_u128(&n, xx, xxx);
    printf("atomic_cas_u128: %d, %ld, %ld\n", res, n.flag, n.data);
    xx = n;
    res = atomic_cas_u128(&n, xx, xxx);
    printf("atomic_cas_u128: %d, %ld, %ld\n", res, n.flag, n.data);

    i64 i = -1;
    printf("atomic_load_i64: %ld\n", (i64)atomic_load_u64(&i));
    atomic_store_u64(&i, -123);
    printf("atomic_load_i64: %ld\n", (i64)atomic_load_u64(&i));
    i = 0;
    printf("atomic_add_i64: %ld\n", atomic_add_u64(&i, -1));

    u64 data;
    scq_push(x, 123);
    scq_push(x, 456);
    printf("scq head, tail = %ld, %ld\n", x->head, x->tail);
    res = scq_pop(x, &data);
    printf("scq pop: %d, %ld\n", res, data);
    res = scq_pop(x, &data);
    printf("scq pop: %d, %ld\n", res, data);
    res = scq_pop(x, &data);
    printf("scq pop: %d, %ld\n", res, data);

    return 1;

    __uint64_t a[2] = {1, 2};
    ucontext_t uctx;
    sigctxt_t ctx;
    ctx.ctxt = &uctx;
    printf("%p\n", &a);
    printf("%p\n", &a[1]);
    printf("%ld\n", (unsigned long)(&a[1]));
    set_rsp(&ctx, (unsigned long)(&a[1]));
    printf("%ld\n", *(unsigned long *)get_rsp(&ctx));

    test_pthread();
    #ifdef ARCH_X64
    printf("ok\n");
    #endif

    
    set_rip(&ctx, 789);
    printf("rax: %ld\n", get_rax(&ctx));
    printf("rip: %ld\n", get_rip(&ctx));
    printf("rsp: %ld\n", get_rsp(&ctx));
    printf("a(*rsp): %ld\n", a[0]);
    push_call(&ctx, 123, 456);
    printf("rsp: %ld\n", get_rsp(&ctx));
    printf("rip: %ld\n", get_rip(&ctx));
    printf("*rsp: %ld\n", *(unsigned long*)get_rsp(&ctx));
    printf("rip: %ld\n", get_rip(&ctx));
    printf("a(*rsp): %ld\n", a[0]);

    dumpgregs(&ctx);
}