#include "klt.h"
#include "scheduler.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

static _Thread_local klt_t *current_klt;

klt_t *klt_current(void) { return current_klt; }
void pco_set_current_klt(klt_t *klt) { current_klt = klt; }

extern int pco_worker_init(klt_t *, const klt_config_t *);
extern void pco_worker_dispose(klt_t *);
extern int pco_worker_stats(klt_t *, klt_stats_t *);

static void *worker_main(void *arg)
{
    klt_t *klt = arg;
    current_klt = klt;
    klt_loop0(klt);
    current_klt = NULL;
    return NULL;
}

klt_t *klt_create_config(const klt_config_t *config)
{
    klt_config_t defaults = {0, 0};
    klt_t *klt = calloc(1, sizeof(*klt));
    int err;
    if (!klt) return NULL;
    init_list_node(&klt->local_list);
    err = pco_worker_init(klt, config ? config : &defaults);
    if (err) { free(klt); errno = err; return NULL; }
    err = pthread_create(&klt->pthread_id, NULL, worker_main, klt);
    if (err) { pco_worker_dispose(klt); free(klt); errno = err; return NULL; }
    return klt;
}

klt_t *klt_create(void) { return klt_create_config(NULL); }
int klt_stats(klt_t *klt, klt_stats_t *stats) { return pco_worker_stats(klt, stats); }

int klt_destroy(klt_t *klt)
{
    int err;
    if (!klt) return EINVAL;
    if (klt_current() == klt || pthread_equal(pthread_self(), klt->pthread_id)) return EDEADLK;
    klt_request_stop(klt);
    err = pthread_join(klt->pthread_id, NULL);
    if (!err) { pco_worker_dispose(klt); free(klt); }
    return err;
}

void dumpklt(klt_t *klt)
{
    if (klt) fprintf(stderr, "worker %lu\n", (unsigned long)klt->pthread_id);
}
