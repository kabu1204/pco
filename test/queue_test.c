#include "util/scq.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

enum { WORKERS = 4, PER_WORKER = 40000, TOTAL = WORKERS * PER_WORKER };

struct test_state {
    lscq_t *queue;
    atomic_uint consumed;
    atomic_uint errors;
    atomic_uint *seen;
};

struct producer_arg { struct test_state *state; unsigned int index; };

static void *producer(void *opaque) {
    struct producer_arg *arg = opaque;
    unsigned int first = arg->index * PER_WORKER;
    for (unsigned int i = 0; i < PER_WORKER; ++i)
        lscq_push(arg->state->queue, first + i);
    return NULL;
}

static void *consumer(void *opaque) {
    struct test_state *state = opaque;
    while (atomic_load(&state->consumed) < TOTAL) {
        u64 value;
        if (!lscq_pop(state->queue, &value)) {
            sched_yield();
            continue;
        }
        if (value >= TOTAL || atomic_fetch_add(&state->seen[value], 1) != 0)
            atomic_fetch_add(&state->errors, 1);
        atomic_fetch_add(&state->consumed, 1);
    }
    return NULL;
}

static int run_case(int concurrent) {
    struct test_state state = {0};
    pthread_t producers[WORKERS], consumers[WORKERS];
    struct producer_arg args[WORKERS];
    state.queue = lscq_create();
    state.seen = calloc(TOTAL, sizeof(*state.seen));
    if (!state.queue || !state.seen) return 1;

    /* Prefilling forces multiple segments; the concurrent case also races
       producers with consumers while segments are linked and advanced. */
    if (concurrent) {
        for (unsigned int i = 0; i < WORKERS; ++i)
            if (pthread_create(&consumers[i], NULL, consumer, &state)) return 1;
    }
    for (unsigned int i = 0; i < WORKERS; ++i) {
        args[i] = (struct producer_arg){ &state, i };
        if (pthread_create(&producers[i], NULL, producer, &args[i])) return 1;
    }
    for (unsigned int i = 0; i < WORKERS; ++i) pthread_join(producers[i], NULL);
    if (!concurrent) {
        for (unsigned int i = 0; i < WORKERS; ++i)
            if (pthread_create(&consumers[i], NULL, consumer, &state)) return 1;
    }
    for (unsigned int i = 0; i < WORKERS; ++i) pthread_join(consumers[i], NULL);
    int failed = atomic_load(&state.errors) != 0;
    for (unsigned int i = 0; i < TOTAL; ++i)
        if (atomic_load(&state.seen[i]) != 1) failed = 1;
    u64 value;
    if (lscq_pop(state.queue, &value)) failed = 1;
    lscq_destroy(state.queue);
    free(state.seen);
    if (failed) fprintf(stderr, "queue lost or duplicated values\n");
    return failed;
}

int main(void) {
    scq_t *segment = scq_create();
    if (!segment) return 1;
    u64 value, initial_head = segment->head;
    /* A negative threshold marks an empty segment and must not advance its
       head on repeated empty polls. */
    for (int i = 0; i < 100; ++i)
        if (scq_pop(segment, &value) || segment->head != initial_head) return 1;
    if (!scq_push(segment, 42) || !scq_pop(segment, &value) || value != 42)
        return 1;
    scq_free(segment);
    return run_case(0) || run_case(1);
}
