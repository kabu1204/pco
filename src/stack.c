#include "stdio.h"
#include "context/context.h"
#include "consts.h"
#include "stddef.h"
#include "malloc.h"
#include "pthread.h"
#include "stack.h"

void ult_entry(tb_context_from_t from) {
    klt_t *klt = pthread_getspecific(klt_k);
    printf("thread_local = %p\n", pthread_getspecific(klt_k));
    printf("klt %ld is running this ult %ld\n", klt->pthread_id, klt->ult->id);
}

int ult_stack_init(ult_stack_t* stk, size_t size){
    if(stk == NULL){
        return -1;
    }

    if(stk->space != NULL) {
        free(stk->space);
    }

    stk->space = malloc(size);

    stk->sp = tb_context_make(stk->space, size, ult_entry);

    return 0;
}



void co_test()
{

}