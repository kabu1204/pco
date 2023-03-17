#ifndef	_STACK_H
#define _STACK_H

#if __cplusplus
extern "C" {
#endif

#include "types.h"

int ult_stack_init(ult_stack_t* stk, tb_context_func_t func, size_t size);

#if __cplusplus
}
#endif

#endif