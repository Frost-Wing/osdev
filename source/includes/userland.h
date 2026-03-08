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

#define USER_CODE_VADDR  0x0000400000000000ULL // canonical user space, isolated PML4 slot
#define USER_HEAP_VADDR  0x0000400010000000ULL // user heap right above code region
#define USER_STACK_TOP   0x00007FFFFFFFF000ULL // near top of canonical lower half

#endif
