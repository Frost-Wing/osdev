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
#include <kernel.h>

/**
 * @brief Halt and catch fire function.
 * 
 */
void hcf() {
    for (;;) {
        #if defined (__x86_64__)
            info("x86_64: Halt Initalized.", __FILE__);
            asm volatile ("hlt");
        #elif defined (__aarch64__) || defined (__riscv)
            info("aarch64 - riscv: Halt (Wait for interrupts) Initalized.", __FILE__);
            asm volatile ("wfi");
        #elif defined (__arm__) || defined (__aarch32__)
            info("ARM32: Halt (Wait for interrupts) Initialized.",__FILE__);
            asm volatile ("wfi");
        #endif
    }
}

/**
 * @brief The clear interrupts command for all architectures.
 * 
 */
void clear_interrupts() {
    for (;;) {
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
}

/**
 * @brief It uses while loops instead of assembly's halt,
 * Good for Userland
 * 
 */
void high_level_halt(){
    while(1){
        // TODO!
    }
}