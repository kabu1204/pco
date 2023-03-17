#include "ult.h"
#include "atomic.h"
#include "context/context.h"
#include "malloc.h"
#include "stack.h"
#include "util/scq.h"
#include "log.h"

static u64 global_id = 0;

extern lscq_t* global_q;

#define new_ult_id() (atomic_add_u64(&global_id, 1))

void ult_exit(tb_context_from_t from) {
    printf("ult exit\n");
    // klt_t *klt = pthread_getspecific(klt_k);
    // printf("thread_local = %p\n", pthread_getspecific(klt_k));
    // printf("klt %ld is running this ult %ld\n", klt->pthread_id, klt->ult->id);
}

ult_t* ult_create(size_t stack_size, ult_func_t entry, void* arg) {
    // TODO(opt): maybe we can use va_list instead of void* in c++
    ult_t* ult = malloc(sizeof(ult_t));
    
    ult->id = new_ult_id();
    ult->bound_klt = 0;

    ult_stack_init(&ult->stk, ult_exit, stack_size);

    ult->stk.sp = context_push_call1(ult->stk.sp, (unsigned long)entry, (unsigned long)arg);
    INFO("injected context pointer: %p\n", ult->stk.sp);
    
    return ult;
}