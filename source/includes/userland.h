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
 
#define USER_STACK_SIZE 0x4000
extern uint8_t user_stack[USER_STACK_SIZE] __attribute__((aligned(16)));

extern void userland_main();
extern void enter_userland();

#endif