/**
 * @file cc-asm.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Cross-compatible ASM header.
 * @version 0.1
 * @date 2023-12-10
 *
 * @copyright Copyright (c) Pradosh 2023-2026
 *
 */
#ifndef CC_ASM_H
#define CC_ASM_H

#include <basics.h>

/**
 * @brief Halt and catch fire function.
 *
 */
void hcf(void);

/**
 * @brief The clear interrupts command for all architectures.
 *
 */
void clear_interrupts(void);

/**
 * @brief The set interrupts command for various architectures.
 *
 */
void set_interrupts(void);

/**
 * @brief It uses while loops instead of assembly's halt,
 * Good for Userland
 *
 */
void high_level_halt(void);

/**
 * @brief Halt and catch fire function but doesn't print any text.
 *
 */
void hcf2(void);

/**
 * @brief Write a value to an x86 model-specific register.
 *
 * @param msr Model-specific register number.
 * @param value Value to write.
 */
void wrmsr64(uint32_t msr, uint64_t value);

/**
 * @brief Read an x86 model-specific register.
 *
 * @param msr Model-specific register number.
 * @return The combined value of the register's EDX:EAX halves.
 */
uint64_t rdmsr64(uint32_t msr);

/**
 * @brief Read the x86 timestamp counter.
 *
 * @return The current timestamp counter value.
 */
uint64_t rdtsc64(void);

#endif // CC_ASM_H
