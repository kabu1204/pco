#include "stdio.h"
#include "context/context.h"
#include "consts.h"
#include "stddef.h"
#include "malloc.h"
#include "pthread.h"
#include "stack.h"

int ult_stack_init(ult_stack_t* stk, tb_context_func_t func, size_t size){
    /* The initial context needs room at the top of the stack. Reject tiny
     * stacks before tb_context_make writes to them. */
    if(stk == NULL || func == NULL || size < 1024){
        return -1;
    }

    char *space = malloc(size);
    if (space == NULL) return -1;
    free(stk->space);
    stk->space = space;

    stk->sp = tb_context_make(stk->space, size, func);

    return 0;
}



void co_test()
{

}
