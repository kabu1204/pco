#include "klt.h"
#include "stdio.h"
#include "sigctx.h"
#include "types.h"
#include <sys/ucontext.h>
#include <unistd.h>

int main(){
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