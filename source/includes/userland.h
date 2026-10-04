/**
 * @file userland.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Header file for userland/userspace.
 * @version 0.1
 * @date 2025-10-03
 *
 * @copyright Copyright (c) Pradosh 2025-2026
 *
 */
#ifndef USERLAND_H
#define USERLAND_H

#include <basics.h>
#include <executables/elf.h>
#include <syscalls.h>

#define USER_STACK_SIZE (8 MiB) // Matches Linux default stack size limit
#define USER_HEAP_SIZE (512 MiB)
#define USER_MMAP_SIZE (1 GiB)

#define USER_CODE_VADDR 0x0000400000000000ULL // canonical user space, isolated PML4 slot
#define USER_INTERP_VADDR 0x0000400008000000ULL // dynamically-linked ELF interpreter base
#define USER_HEAP_VADDR 0x0000400010000000ULL // user heap right above code region
#define USER_MMAP_VADDR (USER_HEAP_VADDR + USER_HEAP_SIZE)
#define USER_TLS_VADDR (USER_MMAP_VADDR + USER_MMAP_SIZE)
#define USER_TLS_REGION_SIZE (16 * 1024)
#define USER_PHDR_VADDR (USER_TLS_VADDR + USER_TLS_REGION_SIZE)
#define USER_PHDR_REGION_SIZE (2 * 4096)
#define USER_STACK_TOP 0x00007FFFFFFFF000ULL // near top of canonical lower half

#define LINUX_AT_NULL 0
#define LINUX_AT_PHDR 3
#define LINUX_AT_PHENT 4
#define LINUX_AT_PHNUM 5
#define LINUX_AT_PAGESZ 6
#define LINUX_AT_BASE 7
#define LINUX_AT_FLAGS 8
#define LINUX_AT_ENTRY 9
#define LINUX_AT_UID 11
#define LINUX_AT_EUID 12
#define LINUX_AT_GID 13
#define LINUX_AT_EGID 14
#define LINUX_AT_PLATFORM 15
#define LINUX_AT_HWCAP 16
#define LINUX_AT_HWCAP2 26
#define LINUX_AT_CLKTCK 17
#define LINUX_AT_SECURE 23
#define LINUX_AT_RANDOM 25
#define LINUX_AT_EXECFN 31
#define IA32_FS_BASE_MSR 0xC0000100
#define USER_AUXV_MAX 20

typedef struct {
    int i[4];
} glibc_128bits_t;

typedef union {
    uint64_t counter;

    struct {
        void *val;
        void *to_free;
    } pointer;
} glibc_dtv_t;

typedef struct {
    uint64_t tcb;
    glibc_dtv_t *dtv;
    uint64_t self;
    uint32_t multiple_threads;
    uint32_t gscope_flag;
    uint64_t sysinfo;
    uint64_t stack_guard;
    uint64_t pointer_guard;
    uint64_t unused_vgetcpu_cache[2];
    uint32_t feature_1;
    int32_t __glibc_unused1;
    void *__private_tm[4];
    void *__private_ss;
    uint64_t ssp_base;
    glibc_128bits_t __glibc_unused2[8][4] __attribute__((aligned(32)));
    void *__padding[8];
} glibc_tcb_head_t;

typedef struct {
    glibc_tcb_head_t head;
    glibc_dtv_t dtv[2];
} glibc_tls_block_t;

typedef struct {
    uint64_t ret_rip; // offset 0
    uint64_t ret_rsp; // offset 8
    uint64_t rbx;     // offset 16
    uint64_t rbp;     // offset 24
    uint64_t r12;     // offset 32
    uint64_t r13;     // offset 40
    uint64_t r14;     // offset 48
    uint64_t r15;     // offset 56
} userland_caller_state_t;

/* Keep these offsets in sync with kernel/asm/userland_exec.S. */
_Static_assert(__builtin_offsetof(userland_caller_state_t, ret_rip) == 0, "caller ret_rip offset mismatch");
_Static_assert(__builtin_offsetof(userland_caller_state_t, ret_rsp) == 8, "caller ret_rsp offset mismatch");
_Static_assert(__builtin_offsetof(userland_caller_state_t, rbx) == 16, "caller rbx offset mismatch");
_Static_assert(__builtin_offsetof(userland_caller_state_t, rbp) == 24, "caller rbp offset mismatch");
_Static_assert(__builtin_offsetof(userland_caller_state_t, r12) == 32, "caller r12 offset mismatch");
_Static_assert(__builtin_offsetof(userland_caller_state_t, r13) == 40, "caller r13 offset mismatch");
_Static_assert(__builtin_offsetof(userland_caller_state_t, r14) == 48, "caller r14 offset mismatch");
_Static_assert(__builtin_offsetof(userland_caller_state_t, r15) == 56, "caller r15 offset mismatch");
_Static_assert(sizeof(userland_caller_state_t) == 64, "caller state size mismatch");

_Static_assert(__builtin_offsetof(glibc_tcb_head_t, stack_guard) == 0x28, "glibc stack_guard offset mismatch");
_Static_assert(__builtin_offsetof(glibc_tcb_head_t, pointer_guard) == 0x30, "glibc pointer_guard offset mismatch");
_Static_assert(__builtin_offsetof(glibc_tcb_head_t, __private_ss) == 0x70, "glibc __private_ss offset mismatch");
_Static_assert(__builtin_offsetof(glibc_tcb_head_t, __glibc_unused2) == 0x80, "glibc __glibc_unused2 offset mismatch");

typedef struct {
    uint64_t key;
    uint64_t value;
} auxv_pair_t;

typedef struct {
    const char *path;
    int argc;
    const char *argv[32];
    const char *const *envp;
} userland_exec_ctx_t;

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;    /* 0..48   */
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15; /* 56..112 */
    uint64_t rip, rflags, rsp;                     /* 120,128,136 */
} userland_regs_t;

#define USERLAND_FORK_FAILED (-2147483647 - 1)

/**
 * @brief Fork the current user task using its saved register state.
 * @param regs Register state to copy into the child.
 * @return Child identifier in the parent, zero in the child, or an error value.
 */
int  userland_fork(const userland_regs_t *regs);

/** @brief Return to user mode using the supplied register state. */
extern void userland_iret_regs(const userland_regs_t *regs) __attribute__((noreturn));

/** @brief Enter user mode at an instruction address. */
void enter_userland_at(uint64_t entry_point);

/**
 * @brief Prepare an executable image and its user stack.
 * @param path Executable path.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param out_info Receives executable metadata.
 * @param out_entry Receives the entry address.
 * @param out_stack Receives the initial stack pointer.
 */
void userland_exec_prepare(
    const char *path,
    int argc,
    const char *argv[],
    elf_image_info_t *out_info,
    void **out_entry,
    uint64_t *out_stack);

/** @brief Initialize the userland heap for the current task. */
void userland_heap_init(void);

/** @brief Adjust the current task's program break. */
uint64_t userland_brk(uint64_t requested_break);

/** @brief Map anonymous user memory and return its address. */
uint64_t userland_mmap_anon(uint64_t length);

/** @brief Unmap a user-memory range. */
bool userland_mmap_unmap(uint64_t addr, uint64_t length);

/** @brief Map user memory at a requested fixed address. */
uint64_t userland_mmap_fixed(uint64_t addr, uint64_t length);

/** @brief Change permissions on a user-memory range. */
bool userland_mprotect(uint64_t addr, uint64_t length, uint64_t prot);

/** @brief Prepare the current user task to exit through the syscall path. */
bool userland_prepare_exit(syscall_frame_t *frame, uint64_t exit_code);

/** @brief Execute a user program from an execution context. */
int userland_exec(const userland_exec_ctx_t *ctx);

/** @brief Check whether userland execution is active. */
bool userland_is_running(void);

/** @brief Abort userland execution after a processor exception. */
void userland_abort_from_exception(uint64_t int_no, uint64_t err_code, uint64_t fault_rip) __attribute__((noreturn));

/** @brief Abort userland execution after a keyboard-requested exit. */
void userland_abort_from_keyboard(int exit_code) __attribute__((noreturn));

/** @brief Replace the current user image with a new program. */
int userland_exec_replace(const userland_exec_ctx_t *ctx);

/** @brief Execute a shell command in userland. */
void sh_exec(void);

#endif
