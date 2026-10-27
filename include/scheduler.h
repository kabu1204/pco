#ifndef PCO_SCHEDULER_H
#define PCO_SCHEDULER_H

#if __cplusplus
extern "C" {
#endif

#include "types.h"
#include "ult.h"
#include "klt.h"

void init_globalq(void);

/* Submit an unsubmitted ULT to a specific worker. Returns 0, EINVAL for
 * invalid arguments or a stopping worker, or EALREADY for a queued/running
 * ULT. The caller must synchronize this operation with klt_destroy. */
int ult_put_localq(ult_t* ult, klt_t* klt);

/* Submit an unsubmitted ULT to the global queue. Returns 0, EINVAL for a
 * null argument, or EALREADY for a queued/running ULT. On success the
 * scheduler owns the object and frees it after the entry function returns;
 * the caller must no longer access it. On failure ownership stays with the
 * caller. These ownership rules also apply to ult_put_localq. */
int ult_put(ult_t* ult);

void klt_loop0(klt_t* klt);

/* Cooperatively yield from the current ULT; a no-op outside a worker. */
void sched(void);

/* Internal current ULT and preemption mask helpers. */
ult_t *ult_current(void);
void pco_preempt_unblock(void);

/* Internal scheduler hooks. */
void ult_finish(void);
void klt_request_stop(klt_t* klt);

#if __cplusplus
}
#endif

#endif