#include "context/context.h"
#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>
static tb_context_t back, child;
static char stack[65536];
static int injected, failures;
static unsigned mxcsr(void) { unsigned v; __asm__ volatile("stmxcsr %0" : "=m"(v)); return v; }
static unsigned cw(void) { unsigned short v; __asm__ volatile("fnstcw %0" : "=m"(v)); return v; }
static void check(int mode) {
 unsigned expected = mode == FE_DOWNWARD ? 1 : mode == FE_UPWARD ? 2 : mode == FE_TOWARDZERO ? 3 : 0;
 if (((mxcsr() >> 13) & 3) != expected || ((cw() >> 10) & 3) != expected) failures++;
}
static void inject(void *arg) { if (arg != &injected) failures++; injected++; check(FE_DOWNWARD); }
static void entry(tb_context_from_t from) {
 back = from.ctx;
 check(FE_DOWNWARD);
 fesetround(FE_TOWARDZERO);
 from = tb_context_jump(back, NULL);
 back = from.ctx;
 check(FE_TOWARDZERO);
 tb_context_jump(back, NULL);
 abort();
}
int main(void) {
 fesetround(FE_DOWNWARD);
 child = tb_context_make(stack, sizeof(stack), entry);
 child = context_push_call1(child, (unsigned long)inject, (unsigned long)&injected);
 fesetround(FE_UPWARD);
 tb_context_from_t from = tb_context_jump(child, NULL);
 child = from.ctx;
 check(FE_UPWARD);
 fesetround(FE_TONEAREST);
 tb_context_jump(child, NULL);
 check(FE_TONEAREST);
 fesetround(FE_TONEAREST);
 if (injected != 1 || failures) { fprintf(stderr, "injected=%d failures=%d\n", injected, failures); return 1; }
 puts("floating-point control and injected call preserved");
 return 0;
}
