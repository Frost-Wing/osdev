/**
 * @file memory.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief All memory based functions
 * @version 0.1
 * @date 2023-10-21
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <stddef.h>
#include <memory.h>

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
 
    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }
 
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
 
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
 
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
 
    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }
 
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
 
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }
 
    return 0;
}

void memory_dump(const void* start, const void* end) {
    const unsigned char* ptr = (const unsigned char*)start;

    while (ptr <= (const unsigned char*)end) {
        printf("Address: 0x%x Value: 0x%x", (void*)ptr, *ptr);
        ptr++;
    }
}

void registers_dump(){
    unsigned long long rax_value, rbx_value, rcx_value, rdx_value, rsi_value, rdi_value, 
                       rbp_value, rsp_value, r8_value, r9_value, r10_value, r11_value, 
                       r12_value, r13_value, r14_value, r15_value;

    asm("movq %%rax, %0" : "=r" (rax_value));
    asm("movq %%rbx, %0" : "=r" (rbx_value));
    asm("movq %%rcx, %0" : "=r" (rcx_value));
    asm("movq %%rdx, %0" : "=r" (rdx_value));
    asm("movq %%rsi, %0" : "=r" (rsi_value));
    asm("movq %%rdi, %0" : "=r" (rdi_value));
    asm("movq %%rbp, %0" : "=r" (rbp_value));
    asm("movq %%rsp, %0" : "=r" (rsp_value));
    asm("movq %%r8, %0" : "=r" (r8_value));
    asm("movq %%r9, %0" : "=r" (r9_value));
    asm("movq %%r10, %0" : "=r" (r10_value));
    asm("movq %%r11, %0" : "=r" (r11_value));
    asm("movq %%r12, %0" : "=r" (r12_value));
    asm("movq %%r13, %0" : "=r" (r13_value));
    asm("movq %%r14, %0" : "=r" (r14_value));
    asm("movq %%r15, %0" : "=r" (r15_value));

    print("Register Dump :\n");
    printf("RAX = 0x%x", rax_value);
    printf("RBX = 0x%x", rbx_value);
    printf("RCX = 0x%x", rcx_value);
    printf("RDX = 0x%x", rdx_value);
    printf("RSI = 0x%x", rsi_value);
    printf("RDI = 0x%x", rdi_value);
    printf("RBP = 0x%x", rbp_value);
    printf("RSP = 0x%x", rsp_value);
    printf("R08 = 0x%x", r8_value);
    printf("R09 = 0x%x", r9_value);
    printf("R10 = 0x%x", r10_value);
    printf("R11 = 0x%x", r11_value);
    printf("R12 = 0x%x", r12_value);
    printf("R13 = 0x%x", r13_value);
    printf("R14 = 0x%x", r14_value);
    printf("R15 = 0x%x", r15_value);

    return 0;
}