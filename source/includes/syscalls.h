/**
 * @file syscalls.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-12-31
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>
#include <isr.h>

enum {
    sys_print = 0,
};

void syscall_handler(InterruptFrame* frame, int syscall_number, ...);
