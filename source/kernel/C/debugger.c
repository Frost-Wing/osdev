/**
 * @file debugger.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief This code uses the E9 hack to make debugging a lot easier
 * @version 0.1
 * @date 2023-10-31
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <debugger.h>

/**
 * @brief If there was possibility for the change in port, we can easily change the E9 Port.
 * 
 */
int port = 0xE9;

/**
 * @brief Puts a character to the address 0xE9 using x86_64 outb
 * 
 * @param c The single character that you want to print.
 */
void debug_putc(char c){
    #if debugger_mode
    outb(port, c);
    #endif
}

/**
 * @brief Puts a string to the address 0xE9 using debug_putc()
 * 
 * @param msg 
 */
void debug_print(const char * msg){
    while(*msg){
        debug_putc(*msg);
        msg++;
    }
}