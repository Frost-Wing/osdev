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
#define USER_TLS_REGION_SIZE (16 * 1024)
#define USER_PHDR_VADDR  (USER_TLS_VADDR + USER_TLS_REGION_SIZE)
#define USER_STACK_TOP   0x00007FFFFFFFF000ULL // near top of canonical lower half

#define LINUX_AT_NULL    0
#define LINUX_AT_PHDR    3
#define LINUX_AT_PHENT   4
#define LINUX_AT_PHNUM   5
#define LINUX_AT_PAGESZ  6
#define LINUX_AT_BASE    7
#define LINUX_AT_FLAGS   8
#define LINUX_AT_ENTRY   9
#define LINUX_AT_UID     11
#define LINUX_AT_EUID    12
#define LINUX_AT_GID     13
#define LINUX_AT_EGID    14
#define LINUX_AT_PLATFORM 15
#define LINUX_AT_HWCAP   16
#define LINUX_AT_HWCAP2  26
#define LINUX_AT_CLKTCK  17
#define LINUX_AT_SECURE  23
#define LINUX_AT_RANDOM  25
#define LINUX_AT_EXECFN  31
#define IA32_FS_BASE_MSR 0xC0000100
#define USER_AUXV_MAX    18

typedef struct {
    int i[4];
} glibc_128bits_t;

typedef union {
    uint64_t counter;
    struct {
        void* val;
        void* to_free;
    } pointer;
} glibc_dtv_t;

typedef struct {
    uint64_t tcb;
    glibc_dtv_t* dtv;
    uint64_t self;
    uint32_t multiple_threads;
    uint32_t gscope_flag;
    uint64_t sysinfo;
    uint64_t stack_guard;
    uint64_t pointer_guard;
    uint64_t unused_vgetcpu_cache[2];
    uint32_t feature_1;
    int32_t __glibc_unused1;
    void* __private_tm[4];
    void* __private_ss;
    uint64_t ssp_base;
    glibc_128bits_t __glibc_unused2[8][4] __attribute__((aligned(32)));
    void* __padding[8];
} glibc_tcb_head_t;

typedef struct {
    glibc_tcb_head_t head;
    glibc_dtv_t dtv[2];
} glibc_tls_block_t;

_Static_assert(__builtin_offsetof(glibc_tcb_head_t, stack_guard) == 0x28, "glibc stack_guard offset mismatch");
_Static_assert(__builtin_offsetof(glibc_tcb_head_t, pointer_guard) == 0x30, "glibc pointer_guard offset mismatch");
_Static_assert(__builtin_offsetof(glibc_tcb_head_t, __private_ss) == 0x70, "glibc __private_ss offset mismatch");
_Static_assert(__builtin_offsetof(glibc_tcb_head_t, __glibc_unused2) == 0x80, "glibc __glibc_unused2 offset mismatch");

typedef struct {
    uint64_t key;
    uint64_t value;
} auxv_pair_t;

void enter_userland_at(uint64_t entry_point);
int userland_exec(const char* path, int argc, const char* const* argv, const char* const* envp);
void userland_heap_init(void);
uint64_t userland_brk(uint64_t requested_break);
uint64_t userland_mmap_anon(uint64_t length);
bool userland_prepare_exit(syscall_frame_t* frame, uint64_t exit_code);
void sh_exec(void);

#endif
