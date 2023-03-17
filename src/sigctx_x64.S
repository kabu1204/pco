.globl preempt_call
.type preempt_call, @function

// remember: we are operating on ult stack
// the preempt_call will be pushed onto a
// ult stack in signal handler. So it will
// not be called explicitly.
// That's why we need to save volatile registers
// (like rax, rcx) as well. 
preempt_call:
    // save user registers
    pushq %rbp
    movq  %rsp, %rbp

    pushfq

    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    // call scheduler function
    callq sched

    // restore user registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax

    popfq
    popq %rbp

    retq