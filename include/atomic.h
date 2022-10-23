#ifndef _PCO_ATOMIC_H
#define _PCO_ATOMIC_H

#include <sys/types.h>
typedef unsigned long u64;


u64 atomic_load_u64(u64* addr);

void atomic_store_u64(u64* addr, u64 val);

int atomic_cas_u64(u64* addr, u64 old, u64 new);

int atomic_bts_u64(u64* addr, u64 shift);

int atomic_btr_u64(u64* addr, u64 shift);

u64 read_rflags();

#endif