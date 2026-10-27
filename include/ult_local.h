#ifndef PCO_ULT_LOCAL_H
#define PCO_ULT_LOCAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stable for the lifetime of a ULT. Returns 0 outside a ULT. */
typedef uint64_t pco_ult_id_t;
pco_ult_id_t pco_ult_self(void);

/* Keys may be reused after deletion without exposing previous ULT values.
 * Key deletion must be synchronized with all operations on that key and with
 * ULTs exiting while it has a non-NULL value. There are at most 256 active keys.
 * A key's destructor runs on ULT exit for a non-NULL value, with the value
 * cleared first. Values installed again by a destructor are handled for at
 * most four passes. Deleting a key does not call destructors. */
typedef uint64_t pco_ult_local_key_t;
int pco_ult_local_key_create(pco_ult_local_key_t *key, void (*destructor)(void *));
int pco_ult_local_key_delete(pco_ult_local_key_t key);
/* These operate on the current ULT, returning EPERM outside a ULT. Values are
 * initially NULL; get writes the value to *out and returns zero on success. */
int pco_ult_local_set(pco_ult_local_key_t key, void *value);
int pco_ult_local_get(pco_ult_local_key_t key, void **out);

/* Runtime hook: called once on the ULT stack before completion. */
struct _ult;
void pco_ult_local_cleanup(struct _ult *ult);

#ifdef __cplusplus
}
#endif
#endif
