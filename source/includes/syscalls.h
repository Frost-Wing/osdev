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

/**
 * @brief Prefix for the syscalls
 * 
 */
#define syscalls_prefix "Syscall Invoked: "

/**
 * @brief Syscall number for the read syscall
 * 
 * @param num Syscall number.
 */
void invoke_syscall(int64 num);

/**
 * @brief Function to handle syscalls.
 * 
 * @param frame The interrupt frame.
 */
void syscalls_handler(InterruptFrame* frame);