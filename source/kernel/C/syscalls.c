/**
 * @file syscalls.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-12-31
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <syscalls.h>

void syscall_handler(InterruptFrame* frame, int syscall_number, ...) {
    va_list argp;
    va_start(argp, syscall_number);
    switch (syscall_number) {
        case sys_print:
            print(va_arg(argp, string));
            break;
    }
    va_end(argp);
}