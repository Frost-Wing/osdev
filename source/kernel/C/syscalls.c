/**
 * @file syscalls.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-01-05
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */
#include <syscalls.h>

void invoke_syscall(int64 num) {
    asm volatile (
        "movq %0, %%rax\n\t"
        "int $0x80\n\t"
        :
        : "g" ((int64)num)
        : "rax"
    );
}

void syscalls_handler(InterruptFrame* frame){
    switch (frame->rax)
    {
        case 1:
            info(syscalls_prefix "syscall id - 1 (test syscalls) has been called!", __FILE__);
            break;
        default:
            error(syscalls_prefix "Unknown", __FILE__);
            hcf2();
            break;
    }

    outb(0x20, 0x20); // End PIC Master
    outb(0xA0, 0x20); // End PIC Slave
}