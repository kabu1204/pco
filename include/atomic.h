#ifndef _PCO_ATOMIC_H
#define _PCO_ATOMIC_H

#if __cplusplus
extern "C" {
#endif

#include "common.h"

u64 atomic_load_u64(u64* addr);

void atomic_store_u64(u64* addr, u64 val);

u64 atomic_add_u64(u64* addr, u64 delta);

int atomic_cas_u64(u64* addr, u64 old, u64 new_);

int atomic_bts_u64(u64* addr, u64 shift);

int atomic_btr_u64(u64* addr, u64 shift);

u64 read_rflags();

u128 atomic_load_u128(u128* addr);

int atomic_cas_u128(u128* addr, u128 old, u128 new_);

#if __cplusplus
}
#endif

#endif