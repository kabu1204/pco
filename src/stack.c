#include "stdio.h"
#include "context/context.h"
#include "consts.h"
#include "stddef.h"
#include "malloc.h"
#include "pthread.h"
#include "stack.h"
#include "log.h"

int ult_stack_init(ult_stack_t* stk, tb_context_func_t func, size_t size){
    if(stk == NULL){
        return -1;
    }

    if(stk->space != NULL) {
        free(stk->space);
    }

    stk->space = malloc(size);
    INFO("stack bottom: %p\n", stk->space);

    stk->sp = tb_context_make(stk->space, size, func);
    INFO("tb context sp: %p\n", stk->sp);

    return 0;
}



void co_test()
{

}