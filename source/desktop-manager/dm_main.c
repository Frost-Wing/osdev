/**
 * @file dm_main.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The main code for Desktop Manager
 * @version 0.1
 * @date 2024-01-07
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */

#include <basics.h> // Avoid headers from kernel, this header contains just some basic macros.

int64* fb_addr = null;
int64* font_addr = null;
int64 width = 0;
int64 height = 0;
int64 pitch = 0;

typedef struct {
    int64* data;
} syscall_result;

/*
* Example syscall usage

(you can add more registers too!)
asm volatile("movq %0, %%rax" :: "r"((int64) [SYSCALL ID] ));
asm volatile ("int $0x80");

Replace [SYSCALL ID] to appropriate syscall numbers.
*/

void send_alive_msg(){
    asm volatile("movq %0, %%rax" :: "r"((int64)0x3));
    asm volatile("int $0x80");
    asm volatile("movq %0, %%rax" :: "r"((int64)0x0)); // ! Always revert the RAX register after an syscall
}

/**
 * @attention Don't rename this function, if you wanted to rename it, u must change the linker also.
 * 
 */
int dw_main(){
    send_alive_msg();

    return 0; // status code
}