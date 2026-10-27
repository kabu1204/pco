#ifndef	_KLT_H
#define _KLT_H

#if __cplusplus
extern "C" {
#endif

#include "types.h"
#include "atomic.h"
#include "signal.h"
#include <stdatomic.h>

#define KLT_FLAG_SCHED_SHIFT 0
#define KLT_FLAG_SCHED 1 << KLT_FLAG_SCHED_SHIFT
#define SIG_RESERVED SIGURG

#define klt_sched_flag(k) (k->flag & KLT_FLAG_SCHED)

klt_t* klt_current(void);

void dumpklt(klt_t* k);

typedef struct {
    unsigned long quantum_ns; /* zero disables preemption */
    size_t max_klts; /* zero: no configured upper bound */
} klt_config_t;

typedef struct {
    unsigned long total_created;
    unsigned long peak_klts;
    unsigned long preemptions; /* interruptions, including immediate resumption */
    unsigned long preempt_switches; /* next ULT differs after preemption */
    unsigned long pool_exhaustions; /* transitions to the configured limit */
    unsigned long signal_errors; /* failed signal deliveries */
    int last_signal_error;
    int pool_growth_error; /* last allocator/pthread_create error, zero if none */
} klt_stats_t;

klt_t* klt_create(void);
/* Process-wide SIGRTMIN+6 is reserved for the runtime. Application code
 * must not install a handler or block this signal while a ULT runs. */
klt_t* klt_create_config(const klt_config_t *config);
int klt_stats(klt_t *klt, klt_stats_t *stats);

/* Stop and join a worker after its started and locally queued ULTs finish.
 * Globally queued ULTs remain available to other or future workers. Returns
 * zero or a pthread error code. May block if a running ULT does not finish. */
int klt_destroy(klt_t* klt);

#if __cplusplus
}
#endif

#endif