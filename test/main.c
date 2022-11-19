#include "klt.h"
#include "stdio.h"
#include "sigctx.h"
#include "types.h"
#include <stddef.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include "util/scq.h"
#include "atomic.h"
#include "common.h"

int main(){
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