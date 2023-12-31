/**
 * @file syscalls.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Contains the system calls for the kernel and desktop manager.
 * @version 0.1
 * @date 2023-12-31
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>
#include <syscalls.h>

void sys_print(const char* message) {
    asm volatile (
        "mov rax, %0\n\t"
        "mov rbx, %1\n\t"
        "int 0x80"
        :
        : "r"(sys_print), "r"(message)
        : "rax", "rbx"
    );
}