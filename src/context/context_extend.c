#include "context/context.h"

tb_context_t context_push_call1(tb_context_t ctx, unsigned long addr, unsigned long arg){
    ctx -= 7*sizeof(void*); // 6 non-volatile registers + 1 return addr
    *((unsigned long*)ctx) = addr;
    *((unsigned long*)ctx + 5) = arg;
    /*
     * NOTICE: after pop the address of context_inject_call, the rsp is not aligned with 16bytes,
     * we will do this in context_inject_call
     */
    *((unsigned long*)ctx + 6) = (unsigned long)context_inject_call;
    return ctx;
}