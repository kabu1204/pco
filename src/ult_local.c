#include "ult_local.h"
#include "scheduler.h"
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_KEY_BITS 8u
#define LOCAL_KEY_COUNT (1u << LOCAL_KEY_BITS)
#define LOCAL_KEY_MASK (LOCAL_KEY_COUNT - 1u)
#define LOCAL_DESTRUCTOR_PASSES 4u

/* Odd generations are allocated; even generations are free. Including the
 * generation in a key prevents values for a deleted key from resurfacing. */
struct key_slot {
    _Atomic uint64_t generation;
    _Atomic(void (*)(void *)) destructor;
};
static struct key_slot keys[LOCAL_KEY_COUNT];

struct local_entry {
    uint64_t generation;
    void *value;
};
struct pco_ult_local_state {
    size_t capacity;
    struct local_entry entries[];
};

static int lookup(pco_ult_local_key_t key, unsigned *index, uint64_t *generation)
{
    *index = (unsigned)(key & LOCAL_KEY_MASK);
    *generation = key >> LOCAL_KEY_BITS;
    return (*generation & 1u) &&
        atomic_load_explicit(&keys[*index].generation, memory_order_acquire) == *generation;
}

pco_ult_id_t pco_ult_self(void)
{
    ult_t *ult = ult_current();
    return ult ? (pco_ult_id_t)ult->id : 0;
}

int pco_ult_local_key_create(pco_ult_local_key_t *key, void (*destructor)(void *))
{
    if (!key) return EINVAL;
    for (unsigned i = 0; i < LOCAL_KEY_COUNT; ++i) {
        uint64_t generation = atomic_load_explicit(&keys[i].generation, memory_order_relaxed);
        if ((generation & 1u) || generation >= (UINT64_MAX >> LOCAL_KEY_BITS) - 1) continue;
        if (atomic_compare_exchange_strong_explicit(&keys[i].generation, &generation, generation + 1,
                                                     memory_order_acquire, memory_order_relaxed)) {
            atomic_store_explicit(&keys[i].destructor, destructor, memory_order_release);
            *key = ((generation + 1) << LOCAL_KEY_BITS) | i;
            return 0;
        }
    }
    return EAGAIN;
}

int pco_ult_local_key_delete(pco_ult_local_key_t key)
{
    unsigned index;
    uint64_t generation;
    if (!lookup(key, &index, &generation)) return EINVAL;
    if (!atomic_compare_exchange_strong_explicit(&keys[index].generation, &generation, generation + 1,
                                                  memory_order_release, memory_order_relaxed)) return EINVAL;
    return 0;
}

int pco_ult_local_get(pco_ult_local_key_t key, void **out)
{
    unsigned index;
    uint64_t generation;
    ult_t *ult = ult_current();
    if (!ult) return EPERM;
    if (!out || !lookup(key, &index, &generation)) return EINVAL;
    struct pco_ult_local_state *state = ult->local_state;
    *out = state && index < state->capacity && state->entries[index].generation == generation
        ? state->entries[index].value : NULL;
    return 0;
}

int pco_ult_local_set(pco_ult_local_key_t key, void *value)
{
    unsigned index;
    uint64_t generation;
    ult_t *ult = ult_current();
    if (!ult) return EPERM;
    if (!lookup(key, &index, &generation)) return EINVAL;
    struct pco_ult_local_state *state = ult->local_state;
    if (!state || index >= state->capacity) {
        size_t previous = state ? state->capacity : 0;
        size_t capacity = previous ? previous : 8;
        while (capacity <= index) capacity *= 2;
        struct pco_ult_local_state *expanded = realloc(state, sizeof(*expanded) + capacity * sizeof(expanded->entries[0]));
        if (!expanded) return ENOMEM;
        memset(expanded->entries + previous, 0, (capacity - previous) * sizeof(expanded->entries[0]));
        expanded->capacity = capacity;
        ult->local_state = state = expanded;
    }
    state->entries[index].generation = generation;
    state->entries[index].value = value;
    return 0;
}

void pco_ult_local_cleanup(ult_t *ult)
{
    for (unsigned pass = 0; pass < LOCAL_DESTRUCTOR_PASSES; ++pass) {
        int called = 0;
        for (size_t i = 0; ult->local_state && i < ult->local_state->capacity; ++i) {
            struct local_entry *entry = &ult->local_state->entries[i];
            void *value = entry->value;
            if (!value || atomic_load_explicit(&keys[i].generation, memory_order_acquire) != entry->generation) continue;
            void (*destructor)(void *) = atomic_load_explicit(&keys[i].destructor, memory_order_acquire);
            if (!destructor) continue;
            entry->value = NULL;
            destructor(value);
            called = 1;
        }
        if (!called) break;
    }
    free(ult->local_state);
    ult->local_state = NULL;
}
