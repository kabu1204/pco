#include "sigctx.h"
#include <string.h>

size_t copy_hex(unsigned long x, char *dst, size_t start, char end) {
    char buf[20] = {end};
    int i = (end=='\0')?0:1, n;
    if(x==0) {
        buf[i++] = '0';
    }
    while(x) {
        unsigned long t = x & (0b1111);
        x >>= 4;
        buf[i++] = (t<10)?('0'+t):('a'+t-10);
    }
    n = i;
    dst[start++] = '0';
    dst[start++] = 'x';
    while(i) dst[start++] = buf[--i];
    dst[start] = '\0';
    return n+2;
}

extern struct sigcontext* regs(sigctxt_t *c) {
#if defined(__linux__) && defined(ARCH64)
    return (struct sigcontext*)(&((ucontext_t*)c->ctxt)->uc_mcontext);
#else
    // TODO: implement me
    #error "not implemented"
    return NULL;
#endif
}

extern void push_call(sigctxt_t *c, unsigned long target_pc, unsigned long resume_pc) {
    pushq(c, resume_pc);
    set_rip(c, target_pc);
}

inline void pushq(sigctxt_t *c, unsigned long x){
    unsigned long sp = get_rsp(c);
    sp -= sizeof(void *);
    *(unsigned long *)((void *)sp) = x;
    set_rsp(c, sp);
}

void popq(sigctxt_t *c, unsigned long *x){
    unsigned long sp = get_rsp(c);
    *x = *(unsigned long *)((void *)sp);
    sp += sizeof(void *);
    set_rsp(c, sp);
}

extern void dumpgregs(sigctxt_t *c) {
    char buf[1024];
    size_t p=0;

    strcpy(buf, "----DUMP REGISTER----\n"); p += 22;
    strcpy(buf+p, "rax    "); p += 7; p += copy_hex(get_rax(c), buf, p, '\n');
    strcpy(buf+p, "rbx    "); p += 7; p += copy_hex(get_rbx(c), buf, p, '\n');
    strcpy(buf+p, "rcx    "); p += 7; p += copy_hex(get_rcx(c), buf, p, '\n');
    strcpy(buf+p, "rdx    "); p += 7; p += copy_hex(get_rdx(c), buf, p, '\n');
    strcpy(buf+p, "rdi    "); p += 7; p += copy_hex(get_rdi(c), buf, p, '\n');
    strcpy(buf+p, "rsi    "); p += 7; p += copy_hex(get_rsi(c), buf, p, '\n');
    strcpy(buf+p, "rbp    "); p += 7; p += copy_hex(get_rbp(c), buf, p, '\n');
    strcpy(buf+p, "rsp    "); p += 7; p += copy_hex(get_rsp(c), buf, p, '\n');
    strcpy(buf+p, "r8     "); p += 7; p += copy_hex(get_r8(c), buf, p, '\n');
    strcpy(buf+p, "r9     "); p += 7; p += copy_hex(get_r9(c), buf, p, '\n');
    strcpy(buf+p, "r10    "); p += 7; p += copy_hex(get_r10(c), buf, p, '\n');
    strcpy(buf+p, "r11    "); p += 7; p += copy_hex(get_r11(c), buf, p, '\n');
    strcpy(buf+p, "r12    "); p += 7; p += copy_hex(get_r12(c), buf, p, '\n');
    strcpy(buf+p, "r13    "); p += 7; p += copy_hex(get_r13(c), buf, p, '\n');
    strcpy(buf+p, "r14    "); p += 7; p += copy_hex(get_r14(c), buf, p, '\n');
    strcpy(buf+p, "r15    "); p += 7; p += copy_hex(get_r15(c), buf, p, '\n');
    strcpy(buf+p, "rip    "); p += 7; p += copy_hex(get_rip(c), buf, p, '\n');
    strcpy(buf+p, "rflags "); p += 7; p += copy_hex(get_rflags(c), buf, p, '\n');
    strcpy(buf+p, "cs     "); p += 7; p += copy_hex(get_cs(c), buf, p, '\n');
    strcpy(buf+p, "fs     "); p += 7; p += copy_hex(get_fs(c), buf, p, '\n');
    strcpy(buf+p, "gs     "); p += 7; p += copy_hex(get_gs(c), buf, p, '\n');
    strcpy(buf+p, "---------------------\n"); p += 22;

    write(1, buf, p);
}