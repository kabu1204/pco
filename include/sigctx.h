#ifndef _SIGCTX_H
#define _SIGCTX_H

#include "types.h"
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include "ucontext.h"
#include "stdalign.h"

struct sigcontext* regs(sigctxt_t *c);

void push_call(sigctxt_t *c, unsigned long target_pc, unsigned long resume_pc);

void pushq(sigctxt_t *c, unsigned long x);

void popq(sigctxt_t *c, unsigned long *x);

void preempt_call();

size_t copy_hex(unsigned long x, char *dst, size_t start, char end);

void dumpgregs(sigctxt_t *c);

#define inject_call0(c, func) {push_call(c, (unsigned long)func, get_rip(c));}

#define get_rax(c) (regs(c)->rax)
#define get_rbx(c) (regs(c)->rbx)
#define get_rcx(c) (regs(c)->rcx)
#define get_rdx(c) (regs(c)->rdx)
#define get_rdi(c) (regs(c)->rdi)
#define get_rsi(c) (regs(c)->rsi)
#define get_rax(c) (regs(c)->rax)
#define get_rbp(c) (regs(c)->rbp)
#define get_rsp(c) (regs(c)->rsp)
#define get_rip(c) (regs(c)->rip)
#define get_r8(c)  (regs(c)->r8)
#define get_r9(c)  (regs(c)->r9)
#define get_r10(c)  (regs(c)->r10)
#define get_r11(c)  (regs(c)->r11)
#define get_r12(c)  (regs(c)->r12)
#define get_r13(c)  (regs(c)->r13)
#define get_r14(c)  (regs(c)->r14)
#define get_r15(c)  (regs(c)->r15)
#define get_rflags(c)  (regs(c)->eflags)
#define get_cs(c)  (regs(c)->cs)
#define get_fs(c)  (regs(c)->fs)
#define get_gs(c)  (regs(c)->gs)

#define set_rax(c, x) {regs(c)->rax = x;}
#define set_rbx(c, x) {regs(c)->rbx = x;}
#define set_rcx(c, x) {regs(c)->rcx = x;}
#define set_rdx(c, x) {regs(c)->rdx = x;}
#define set_rdi(c, x) {regs(c)->rdi = x;}
#define set_rsi(c, x) {regs(c)->rsi = x;}
#define set_rax(c, x) {regs(c)->rax = x;}
#define set_rbp(c, x) {regs(c)->rbp = x;}
#define set_rsp(c, x) {regs(c)->rsp = x;}
#define set_rip(c, x) {regs(c)->rip = x;}

#endif