/**
 * @file debugger.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The headers files for E9 hack.
 * @version 0.1
 * @date 2023-10-31
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <stddef.h>
#include <hal.h>

/**
 * @brief Enables or disables the debugger.
 * [1 - Enable]
 * [0 - Disable]
 * 
 */
#define debugger_mode 1

/**
 * @brief Puts a character to the address 0xE9 using x86_64 outb
 * 
 * @param c The single character that you want to print.
 */
void debug_putc(char c);

/**
 * @brief Puts a string to the address 0xE9 using debug_putc()
 * 
 * @param msg 
 */
void debug_print(const char * msg);