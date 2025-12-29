/**
 * @file userland.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Header file for userland/userspace.
 * @version 0.1
 * @date 2025-10-03
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */
#ifndef USERLAND_H
#define USERLAND_H

#include <basics.h>
#include <syscalls.h>

#define USER_STACK_SIZE  (16 * 1024)
#define USER_HEAP_SIZE   (1 * 1024 * 1024)

#define USER_CODE_VADDR  0x40000000ULL   // 1 GB
#define USER_HEAP_VADDR  0x50000000ULL   // 1.25 GB
#define USER_STACK_TOP   0x7FFFFFF000ULL // top of canonical user space

#endif
