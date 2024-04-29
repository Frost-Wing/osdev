/**
 * @file cc-asm.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Cross-compatible ASM header.
 * @version 0.1
 * @date 2023-12-10
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>

/**
 * @brief Halt and catch fire function.
 * 
 */
void hcf();

/**
 * @brief The clear interrupts command for all architectures.
 * 
 */
void clear_interrupts();

/**
 * @brief The set interrupts command for various architectures.
 * 
 */
void set_interrupts();

/**
 * @brief It uses while loops instead of assembly's halt,
 * Good for Userland
 * 
 */
void high_level_halt();

/**
 * @brief Halt and catch fire function but doesn't print any text.
 * 
 */
void hcf2();