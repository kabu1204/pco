#include "ult.h"
#include "atomic.h"
#include "scheduler.h"
#include "stack.h"
#include "ult_local.h"
#include <errno.h>
#include <stdlib.h>

static u64 global_id = 0;

/* A new context starts on its own stack. The worker passes the ULT as the
 * private argument, and the context returned by the first jump is the
 * worker's scheduler stack. */
void ult_exit(tb_context_from_t from) {
    ult_t *ult = (ult_t *)from.priv;
    ult->sched_ctx = from.ctx;
    pco_preempt_unblock();
    ult->entry(ult->arg);
    pco_ult_local_cleanup(ult);
    ult_finish();
    abort(); /* ult_finish must never return to a finished stack. */
}

ult_t* ult_create(size_t stack_size, ult_func_t entry, void* arg) {
    if (entry == NULL) return NULL;

    ult_t *ult = calloc(1, sizeof(*ult));
    if (ult == NULL) return NULL;
    if (ult_stack_init(&ult->stk, ult_exit, stack_size) != 0) {
        free(ult);
        return NULL;
    }

    ult->id = atomic_add_u64(&global_id, 1);
    ult->entry = entry;
    ult->arg = arg;
    return ult;
}

int ult_destroy(ult_t *ult) {
    if (!ult) return EINVAL;
    if (ult->queue_state != 0) return EBUSY;
    free(ult->stk.space);
    free(ult);
    return 0;
}
