#ifndef	_ULT_H
#define _ULT_H

#if __cplusplus
extern "C" {
#endif

#include "types.h"
#include "common.h"
#include "context/context.h"

void ult_exit(tb_context_from_t from);

ult_t* ult_create(size_t stack_size, ult_func_t entry, void* arg);

#if __cplusplus
}
#endif

#endif