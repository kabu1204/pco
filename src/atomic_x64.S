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
//     return *addr;
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
    setz %al
    movzbl %al, %eax
    retq

// atomically behaves as follows:
// atomic_bts_u64(u64* addr, u64 shift):
//     ret = *addr & (1 << shift);
//     *addr &= (1 << shift);
//     return ret;
atomic_bts_u64:
    xorl %eax, %eax
    LOCK
    btsq %rsi, (%rdi)
    # movq $0, %rax
    setc %al
    retq

atomic_btr_u64:
    xorl %eax, %eax
    LOCK
    btrq %rsi, (%rdi)
    # movq $0, %rax
    setc %al
    retq

read_rflags:
    pushfq
    addq $8, %rsp
    movq (%rsp), %rax
    retq

atomic_load_u128:
    movq %rbx, %rax
    movq %rcx, %rdx
    LOCK
    cmpxchg16b (%rdi)
    retq

// %rdi addr
// %rsi old1 => RDX
// %rdx old2 => RAX
// %rcx new1 => RCX
// %r8  new2 => RBX
atomic_cas_u128:
    pushq   %rbx
    movq    %rcx, %rbx
    movq    %rsi, %rax
    xorl    %esi, %esi
    movq    %r8, %rcx
    lock
    cmpxchg16b      (%rdi)
    sete    %sil
    movl    %esi, %eax
    popq    %rbx
    retq