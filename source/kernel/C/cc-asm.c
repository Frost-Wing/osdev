/**
 * @file cc-asm.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The assembly functions defined in C because to run assembly code machine independently.
 * @version 0.1
 * @date 2023-10-23
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <cc-asm.h>

/**
 * @brief Halt and catch fire function.
 * 
 */
void hcf() {
    for (;;) {
        #if defined (__x86_64__)
            // info("x86_64: Halt Initalized.", __FILE__);
            asm volatile ("hlt");
        #elif defined (__aarch64__) || defined (__riscv)
            // info("aarch64 - riscv: Halt (Wait for interrupts) Initalized.", __FILE__);
            asm volatile ("wfi");
        #elif defined (__arm__) || defined (__aarch32__)
            // info("ARM32: Halt (Wait for interrupts) Initialized.",__FILE__);
            asm volatile ("wfi");
        #endif
    }
}

/**
 * @brief Halt and catch fire function but doesn't print any text.
 * 
 */
void hcf2() {
    for (;;) {
        #if defined (__x86_64__)
            asm volatile ("hlt");
        #elif defined (__aarch64__) || defined (__riscv
            asm volatile ("wfi");
        #elif defined (__arm__) || defined (__aarch32__)
            asm volatile ("wfi");
        #endif
    }
}

/**
 * @brief The clear interrupts command for all architectures.
 * 
 */
void clear_interrupts() {
    #if defined(__x86_64__)
        info("x86_64: Cleared interrupts.", __FILE__);
        asm volatile ("cli");
    #elif defined(__aarch64__) || defined(__riscv)
        info("aarch64 - riscv: Cleared interrupts.", __FILE__);
        asm volatile ("msr daifset, #2");
    #elif defined(__arm__) || defined(__aarch32__)
        info("ARM32: Cleared interrupts.", __FILE__);
        asm volatile ("cpsid i");
    #endif
}

/**
 * @brief The set interrupts command for various architectures.
 * 
 */
void set_interrupts() {
    #if defined(__x86_64__)
        info("x86_64: Set interrupts.", __FILE__);
        asm volatile ("sti");
    #elif defined(__aarch64__) || defined(__riscv)
        info("aarch64 - riscv: Set interrupts.", __FILE__);
        asm volatile ("msr daifclr, #2");
    #elif defined(__arm__) || defined(__aarch32__)
        info("ARM32: Set interrupts.", __FILE__);
        asm volatile ("cpsie i");
    #endif
}


/**
 * @brief It uses while loops instead of assembly's halt.
 * 
 */
void high_level_halt(){
    while(1){
        asm volatile ("pause");
    }
}

void wrmsr64(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

uint64_t rdmsr64(uint32_t msr) {
    uint32_t low = 0;
    uint32_t high = 0;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

uint64_t rdtsc64(void) {
    uint32_t low = 0;
    uint32_t high = 0;
    asm volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}