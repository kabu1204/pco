.text

.globl atomic_load_u64
.type atomic_load_u64, @function

.globl atomic_store_u64
.type atomic_store_u64, @function

.globl atomic_add_u64
.type atomic_add_u64, @function

.globl atomic_cas_u64
.type atomic_cas_u64, @function

.globl atomic_bts_u64
.type atomic_bts_u64, @function

.globl atomic_btr_u64
.type atomic_btr_u64, @function

.globl read_rflags
.type read_rflags, @function

.globl atomic_load_u128
.type atomic_load_u128, @function

.globl atomic_cas_u128
.type atomic_cas_u128, @function

atomic_load_u64:
    movq (%rdi), %rax
    retq

atomic_store_u64:
    xchgq %rsi, (%rdi)
    retq

// atomically behaves as follows:
// atomic_add_u64(u64 *addr, u64 delta):
//     *addr += delta;
//     return *delta;
atomic_add_u64:
    movq %rsi, %rax
    LOCK
    xaddq %rax, (%rdi)
    addq %rsi, %rax
    retq

// atomically behaves as follows:
// atomic_cas_u64(u64 *addr, u64 old, u64 new):
//     if(*addr == old)
//     {
//         *addr = new;
//         return 1;
//     }
//     else return 0;
atomic_cas_u64:
    movq %rsi, %rax
    LOCK
    cmpxchgq %rdx, (%rdi)
    sete %al
    retq

// atomically behaves as follows:
// atomic_bts_u64(u64* addr, u64 shift):
//     ret = *addr & (1 << shift);
//     *addr &= (1 << shift);
//     return ret;
atomic_bts_u64:
    LOCK
    btsq %rsi, (%rdi)
    movq $0, %rax
    setc %al
    retq

atomic_btr_u64:
    LOCK
    btrq %rsi, (%rdi)
    movq $0, %rax
    setc %al
    retq

read_rflags:
    pushfq
    addq $8, %rsp
    movq (%rsp), %rax
    retq

atomic_load_u128:
    movq %rax, %rbx
    movq %rdx, %rcx
    LOCK
    cmpxchg16b (%rdi)
    retq

// %rdi addr
// %rsi old1
// %rdx old2
// %rcx new1
// %r8  new2
atomic_cas_u128:
    movq %rsi, %rax
    movq %rcx, %rbx
    movq %r8, %rcx
    LOCK
    cmpxchg16b (%rdi)
    sete %al
    retq