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
 
#define USER_STACK_SIZE 0x4000      // 16 KB
#define USER_STACK_VADDR 0x70000000
#define USER_HEAP_VADDR 0x50000000
#define USER_HEAP_SIZE  0x100000   // 1 MB
#define USER_CODE_VADDR  0x00400000ULL
#define USER_STACK_TOP   0x00007FFFFFF000ULL


extern void userland_main();
extern void enter_userland();

#endif