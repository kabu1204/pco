#include "atomic.h"
#include "common.h"
#include <stdio.h>

#define EXPECT_TRUE(expr, msg) { \
    if(!(expr)){ \
        printf("[%s:%d] " #expr " is FALSE\n\t%s\n", __FILE__, __LINE__, msg); \
    }   \
}

#define TEST_FINISH() { printf("%s FINISHED\n", __FUNCTION__); }

int eq_u128(const u128 l, const u128 r){
    return l.flag==r.flag && l.data==r.data;
}

void test_atomic_load_u64(){
    u64 a = 123;
    EXPECT_TRUE(atomic_load_u64(&a)==123, "");
    TEST_FINISH();
}

void test_atomic_store_u64(){
    u64 a = 123;
    EXPECT_TRUE(atomic_load_u64(&a)==123, "");
    atomic_store_u64(&a, 456);
    EXPECT_TRUE(atomic_load_u64(&a)==456, "");
    TEST_FINISH();
}

void test_atomic_add_u64(){
    u64 a = 123;
    EXPECT_TRUE(atomic_load_u64(&a)==123, "");
    EXPECT_TRUE(atomic_add_u64(&a, 1)==124, "");
    EXPECT_TRUE(atomic_add_u64(&a, 124)==248, "");
    EXPECT_TRUE(atomic_add_u64(&a, 0)==248, "");
    EXPECT_TRUE(atomic_add_u64(&a, -1)==247, "");
    EXPECT_TRUE(atomic_add_u64(&a, -247)==0, "");
    TEST_FINISH();
}

void test_atomic_cas_u64(){
    u64 a = 0x12345678, b = 0x456789ab;
    EXPECT_TRUE(atomic_load_u64(&a)==0x12345678, "");
    EXPECT_TRUE(atomic_load_u64(&b)==0x456789ab, "");
    asm volatile("movq $0xffffffff, %rax\n");
    EXPECT_TRUE(atomic_cas_u64(&a, 0x12345677, 321)==0, "");
    asm volatile("movq $0xffffffff, %rax\n");
    EXPECT_TRUE(atomic_cas_u64(&a, 0x12345678, 321)==1, "");
    EXPECT_TRUE(atomic_load_u64(&a)==321, "");
    TEST_FINISH();
}

void test_atomic_bit_test_u64(){
    u64 a = 0;
    asm volatile("movq $0xffffffff, %rax\n");
    EXPECT_TRUE(atomic_bts_u64(&a, 0)==0, "");
    EXPECT_TRUE(atomic_load_u64(&a)==1, "");
    asm volatile("movq $0xffffffff, %rax\n");
    EXPECT_TRUE(atomic_btr_u64(&a, 0)==1, "");
    EXPECT_TRUE(atomic_load_u64(&a)==0, "");
    EXPECT_TRUE(atomic_bts_u64(&a, 63)==0, "");
    EXPECT_TRUE(atomic_bts_u64(&a, 63)==1, "");
    EXPECT_TRUE(atomic_load_u64(&a)==(1ul<<63), "");
    EXPECT_TRUE(atomic_bts_u64(&a, 62)==0, "");
    EXPECT_TRUE(atomic_load_u64(&a)==(0b11ul<<62), "");
    EXPECT_TRUE(atomic_btr_u64(&a, 63)==1, "");
    EXPECT_TRUE(atomic_load_u64(&a)==(1ul<<62), "");
    EXPECT_TRUE(atomic_btr_u64(&a, 61)==0, "");
    EXPECT_TRUE(atomic_load_u64(&a)==(1ul<<62), "");
    EXPECT_TRUE(atomic_btr_u64(&a, 62)==1, "");
    EXPECT_TRUE(atomic_load_u64(&a)==0, "");
    EXPECT_TRUE(atomic_btr_u64(&a, 62)==0, "");
    EXPECT_TRUE(atomic_load_u64(&a)==0, "");
    TEST_FINISH();
}

void test_atomic_load_u128(){
    u128 a = {.data=123, .flag=456};
    u128 b = {.data=0,   .flag=0};
    u64 rbx1=0xffffffff, rbx2=0;
    asm volatile(
        "movq %%rbx, %0\n"
        : "=m"(rbx1)
        ::"memory"
    );
    b = atomic_load_u128(&a);
    asm volatile(
        "movq %%rbx, %0\n"
        : "=m"(rbx2)
        ::"memory"
    );
    EXPECT_TRUE(rbx1==rbx2, "");    // System V calling convention: rbx is non-volatile register.
    EXPECT_TRUE(eq_u128(b, a), "");
    TEST_FINISH();
}

void test_atomic_cas_u128(){
    u128 a = {.data=123, .flag=456};
    u128 b = {.data=789, .flag=101112};
    u128 c = a;
    u64 rbx1=0xffffffff, rbx2=0, rax1=0xffffffff, rax2=0;
    u64 rdx = 0;
    int res;
    asm volatile(
        "movq %%rbx, %0\n\t"
        "movq %%rax, %1\n\t"
        : "=m"(rbx1), "=m"(rax1)
        ::"memory"
    );
    res = atomic_cas_u128(&a, b, b);
    asm volatile(
        "movq %%rbx, %0\n\t"
        "movq %%rax, %1\n\t"
        "movq %%rdx, %2\n\t"
        : "=m"(rbx2), "=m"(rax2), "=m"(rdx)
        ::"memory"
    );
    EXPECT_TRUE(rbx1==rbx2, "");
    EXPECT_TRUE(rdx==123, "");
    EXPECT_TRUE(res==0, "");
    EXPECT_TRUE(eq_u128(atomic_load_u128(&a), c), "");

    asm volatile(
        "movq %%rbx, %0\n\t"
        "movq %%rax, %1\n\t"
        : "=m"(rbx1), "=m"(rax1)
        ::"memory"
    );
    res = atomic_cas_u128(&a, c, b);
    asm volatile(
        "movq %%rbx, %0\n\t"
        "movq %%rax, %1\n\t"
        "movq %%rdx, %2\n\t"
        : "=m"(rbx2), "=m"(rax2), "=m"(rdx)
        ::"memory"
    );
    EXPECT_TRUE(rdx==123, "");
    EXPECT_TRUE(rbx1==rbx2, "");
    EXPECT_TRUE(res==1, "");
    EXPECT_TRUE(eq_u128(atomic_load_u128(&a), b), "");

    TEST_FINISH();
}

int main(){
    test_atomic_load_u64();
    test_atomic_store_u64();
    test_atomic_add_u64();
    test_atomic_cas_u64();
    test_atomic_bit_test_u64();
    test_atomic_load_u128();
    test_atomic_cas_u128();
}