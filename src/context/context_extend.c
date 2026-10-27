#include "context/context.h"
#include "context/arch.h"
#include <string.h>

tb_context_t context_push_call1(tb_context_t ctx, unsigned long addr, unsigned long arg){
#if defined(ARCH_X64) && !defined(_WIN32)
    tb_context_t original = ctx;
    ctx -= 8*sizeof(void*); // floating-point controls, 6 registers, return address
    memcpy(ctx, original, 6);
    *((unsigned long*)ctx + 1) = addr;
    *((unsigned long*)ctx + 6) = arg;
    // context_inject_call maintains the SysV stack alignment.
    *((unsigned long*)ctx + 7) = (unsigned long)context_inject_call;
#else
    ctx -= 7*sizeof(void*);
    *((unsigned long*)ctx) = addr;
    *((unsigned long*)ctx + 5) = arg;
    *((unsigned long*)ctx + 6) = (unsigned long)context_inject_call;
#endif
    return ctx;
}
