#include "atomic.h"
#include "common.h"
#include <gtest/gtest.h>
#include <stdio.h>

// #define EXPECT_TRUE(expr, msg) { \
//     if(!(expr)){ \
//         printf("[%s:%d] " #expr " is FALSE\n\t%s\n", __FILE__, __LINE__, msg); \
//     }   \
// }

#define TEST_FINISH() { printf("%s FINISHED\n", __FUNCTION__); }

int eq_u128(const u128 l, const u128 r){
    return l.flag==r.flag && l.data==r.data;
}

TEST(AtomicTest, atomic_load_u64){
    u64 a = 123;
    EXPECT_EQ(atomic_load_u64(&a), 123);
}

TEST(AtomicTest, atomic_store_u64){
    u64 a = 123;
    EXPECT_EQ(atomic_load_u64(&a), 123);
    atomic_store_u64(&a, 456);
    EXPECT_EQ(atomic_load_u64(&a), 456);
}

TEST(AtomicTest, atomic_add_u64){
    u64 a = 123;
    EXPECT_EQ(atomic_load_u64(&a), 123);
    EXPECT_EQ(atomic_add_u64(&a, 1), 124);
    EXPECT_EQ(atomic_add_u64(&a, 124), 248);
    EXPECT_EQ(atomic_add_u64(&a, 0), 248);
    EXPECT_EQ(atomic_add_u64(&a, -1), 247);
    EXPECT_EQ(atomic_add_u64(&a, -247), 0);
}

TEST(AtomicTest, atomic_cas_u64){
    u64 a = 0x12345678, b = 0x456789ab;
    EXPECT_EQ(atomic_load_u64(&a), 0x12345678);
    EXPECT_EQ(atomic_load_u64(&b), 0x456789ab);
    asm volatile("movq $0xffffffff, %rax\n");
    EXPECT_EQ(atomic_cas_u64(&a, 0x12345677, 321), 0);
    asm volatile("movq $0xffffffff, %rax\n");
    EXPECT_EQ(atomic_cas_u64(&a, 0x12345678, 321), 1);
    EXPECT_EQ(atomic_load_u64(&a), 321);
}

TEST(AtomicTest, atomic_bit_test_u64){
    u64 a = 0;
    asm volatile("movq $0xffffffff, %rax\n");
    EXPECT_TRUE(atomic_bts_u64(&a, 0)==0);
    EXPECT_TRUE(atomic_load_u64(&a)==1);
    asm volatile("movq $0xffffffff, %rax\n");
    EXPECT_TRUE(atomic_btr_u64(&a, 0)==1);
    EXPECT_TRUE(atomic_load_u64(&a)==0);
    EXPECT_TRUE(atomic_bts_u64(&a, 63)==0);
    EXPECT_TRUE(atomic_bts_u64(&a, 63)==1);
    EXPECT_TRUE(atomic_load_u64(&a)==(1ul<<63));
    EXPECT_TRUE(atomic_bts_u64(&a, 62)==0);
    EXPECT_TRUE(atomic_load_u64(&a)==(0b11ul<<62));
    EXPECT_TRUE(atomic_btr_u64(&a, 63)==1);
    EXPECT_TRUE(atomic_load_u64(&a)==(1ul<<62));
    EXPECT_TRUE(atomic_btr_u64(&a, 61)==0);
    EXPECT_TRUE(atomic_load_u64(&a)==(1ul<<62));
    EXPECT_TRUE(atomic_btr_u64(&a, 62)==1);
    EXPECT_TRUE(atomic_load_u64(&a)==0);
    EXPECT_TRUE(atomic_btr_u64(&a, 62)==0);
    EXPECT_TRUE(atomic_load_u64(&a)==0);
}

TEST(AtomicTest, atomic_load_u128){
    u128 a = {.flag=456, .data=123};
    u128 b = {.flag=0,   .data=0};
    u64 rbx=0xffffffff;
    bool rbx_equal = false;
    asm volatile(
        "movq %%rbx, %3\n\t"
        "movq %4, %%rdi\n\t"
        "call atomic_load_u128\n\t"
        "movq %%rdx, %1\n\t"
        "movq %%rax, %2\n\t"
        "xorb %b0, %b0\n\t"
        "cmpq %%rbx, %3\n\t"
        "setz %b0\n\t"
        : "=r"(rbx_equal), "=m"(b.data), "=m"(b.flag), "+m"(rbx)
        : "r"(&a)
        : "memory"
    );
    EXPECT_TRUE(rbx_equal);    // System V calling convention: rbx is non-volatile register.
    EXPECT_TRUE(eq_u128(b, a));
}

TEST(AtomicTest, atomic_cas_u128){
    u128 a = {.flag=456,    .data=123};
    u128 b = {.flag=101112, .data=789};
    u128 c = a;
    u64 rbx=0xffffffff;
    u64 rdx = 0;
    bool rbx_equal = false;
    int res;
    asm volatile(
        "movq %%rbx, %1\n\t"
        "movq %4, %%rdi\n\t"
        "movq %5, %%rsi\n\t"
        "movq %6, %%rdx\n\t"
        "movq %%rsi, %%rcx\n\t"
        "movq %%rdx, %%r8\n\t"
        "call atomic_cas_u128\n\t"
        "movq %%rdx, %3\n\t"
        "movl %%eax, %2\n\t"
        "xorb %0, %0\n\t"
        "cmpq %%rbx, %1\n\t"
        "setz %b0\n\t"
        : "=a"(rbx_equal), "+m"(rbx), "=m"(res), "=m"(rdx)
        : "r"(&a), "m"(b.flag), "m"(b.data)
        : "memory"
    );
    EXPECT_TRUE(rbx_equal);
    EXPECT_TRUE(rdx==123);
    EXPECT_TRUE(res==0);
    EXPECT_TRUE(eq_u128(atomic_load_u128(&a), c));

    asm volatile(
        "movq %%rbx, %1\n\t"
        "movq %4, %%rdi\n\t"
        "movq %5, %%rsi\n\t"
        "movq %6, %%rdx\n\t"
        "movq %7, %%rcx\n\t"
        "movq %8, %%r8\n\t"
        "call atomic_cas_u128\n\t"
        "movq %%rdx, %3\n\t"
        "movl %%eax, %2\n\t"
        "xorb %0, %0\n\t"
        "cmpq %%rbx, %1\n\t"
        "setz %b0\n\t"
        : "=a"(rbx_equal), "+m"(rbx), "=m"(res), "=m"(rdx)
        : "r"(&a), "m"(c.flag), "m"(c.data), "m"(b.flag), "m"(b.data)
        : "memory"
    );
    EXPECT_TRUE(rbx_equal);
    EXPECT_TRUE(rdx==123);
    EXPECT_TRUE(res==1);
    EXPECT_TRUE(eq_u128(atomic_load_u128(&a), b));
}