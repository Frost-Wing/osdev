/**
 * @file syscalls.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-01-05
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */
#include <basics.h>
#include <isr.h>

#define syscalls_prefix "Syscall Invoked: "

void invoke_syscall(int64 num);

void syscalls_handler(InterruptFrame* frame);