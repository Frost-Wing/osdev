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
#define USER_MMAP_SIZE   (4 * 1024 * 1024)

#define USER_CODE_VADDR  0x0000400000000000ULL // canonical user space, isolated PML4 slot
#define USER_HEAP_VADDR  0x0000400010000000ULL // user heap right above code region
#define USER_MMAP_VADDR  (USER_HEAP_VADDR + USER_HEAP_SIZE)
#define USER_TLS_VADDR   (USER_MMAP_VADDR + USER_MMAP_SIZE)
#define USER_PHDR_VADDR  (USER_TLS_VADDR + 0x1000ULL)
#define USER_STACK_TOP   0x00007FFFFFFFF000ULL // near top of canonical lower half

void enter_userland_at(uint64_t entry_point);
int userland_exec(const char* path, int argc, const char* const* argv, const char* const* envp);
void userland_heap_init(void);
uint64_t userland_brk(uint64_t requested_break);
uint64_t userland_mmap_anon(uint64_t length);
void sh_exec(void);

#endif
