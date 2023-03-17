#include "ult_queue.h"
#include "util/scq.h"

int pop_ult(lscq_t* q, ult_t** u){
    return lscq_pop(q, (u64*)u);
}

void push_ult(lscq_t* q, ult_t* u){
    lscq_push(q, (u64)u);
}