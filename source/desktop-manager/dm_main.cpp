/**
 * @file dm_main.cpp
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The main code for Desktop Manager
 * @version 0.1
 * @date 2024-01-07
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */

#include <basics.h> // Avoid headers from kernel, this header contains just some basic macros.

typedef struct
{
    int64* fb_addr;
    int64 width;
    int64 height;
    int64 pitch;
    void (*print)(cstring msg);
} kernel_data ;

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
}

/**
 * @attention Don't rename this function, if you wanted to rename it, u must change the linker also.
 * 
 */
int dw_main(kernel_data* data){
    send_alive_msg();

    return 0; // status code
}