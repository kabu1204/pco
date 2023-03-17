#ifndef PCO_ULT_QUEUE
#define PCO_ULT_QUEUE

#if __cplusplus
extern "C" {
#endif

#include "types.h"
#include "util/list.h"
#include "util/scq.h"

struct ult_list_node {
    ult_t* u;
    struct list_node node;
};

#define ult_entry(ptr) container_of(ptr, struct ult_list_node, node)

int pop_ult(lscq_t* q, ult_t** u);

void push_ult(lscq_t* q, ult_t* u);

#if __cplusplus
}
#endif

#endif