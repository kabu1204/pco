#include "ult.h"
#include "stdatomic.h"
#include "malloc.h"
#include "stack.h"

static atomic_ulong global_id = 0;

#define new_ult_id() (++global_id)

ult_t* ult_create(size_t size) {
    ult_t* ult = malloc(sizeof(ult_t));
    
    ult->id = new_ult_id();

    ult_stack_init(&ult->stk, size);
    
    return ult;
}