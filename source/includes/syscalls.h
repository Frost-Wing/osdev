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
#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <basics.h>
#include <isr.h>

#define PRAD_MAGIC 0xBADF00D

#define LINUX_SYS_READ              0
#define LINUX_SYS_WRITE             1
#define LINUX_SYS_OPEN              2
#define LINUX_SYS_CLOSE             3
#define LINUX_SYS_STAT              4
#define LINUX_SYS_FSTAT             5
#define LINUX_SYS_LSEEK             8
#define LINUX_SYS_MMAP              9
#define LINUX_SYS_MPROTECT          10
#define LINUX_SYS_MUNMAP            11
#define LINUX_SYS_BRK               12
#define LINUX_SYS_RT_SIGACTION      13
#define LINUX_SYS_RT_SIGPROCMASK    14
#define LINUX_SYS_IOCTL             16
#define LINUX_SYS_ACCESS            21
#define LINUX_SYS_WRITEV            20
#define LINUX_SYS_DUP               32
#define LINUX_SYS_DUP2              33
#define LINUX_SYS_NANOSLEEP         35
#define LINUX_SYS_GETPID            39
#define LINUX_SYS_FORK              57
#define LINUX_SYS_EXECVE            59
#define LINUX_SYS_EXIT              60
#define LINUX_SYS_CHDIR             80
#define LINUX_SYS_UNAME             63
#define LINUX_SYS_FCNTL             72
#define LINUX_SYS_GETCWD            79
#define LINUX_SYS_READLINK          89
#define LINUX_SYS_UMASK             95
#define LINUX_SYS_GETUID            102
#define LINUX_SYS_GETEUID           107
#define LINUX_SYS_GETGID            104
#define LINUX_SYS_GETEGID           108
#define LINUX_SYS_GETPPID           110
#define LINUX_SYS_SIGALTSTACK       131
#define LINUX_SYS_ARCH_PRCTL        158
#define LINUX_SYS_GETTID            186
#define LINUX_SYS_FUTEX             202
#define LINUX_SYS_GETDENTS64        217
#define LINUX_SYS_SET_TID_ADDRESS   218
#define LINUX_SYS_CLOCK_GETTIME     228
#define LINUX_SYS_OPENAT            257
#define LINUX_SYS_NEWFSTATAT        262
#define LINUX_SYS_READLINKAT        267
#define LINUX_SYS_FACCESSAT         269
#define LINUX_SYS_SET_ROBUST_LIST   273
#define LINUX_SYS_PRLIMIT64         302
#define LINUX_SYS_GETRANDOM         318
#define LINUX_SYS_STATX             332
#define LINUX_SYS_EXIT_GROUP        231
#define LINUX_SYS_TGKILL            234

#define LINUX_EAGAIN 11

typedef struct syscall_frame {
    uint64_t r9;
    uint64_t r8;
    uint64_t r10;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rax;

    uint64_t rip;     // rcx
    uint64_t cs;      // 0x1B
    uint64_t rflags;  // r11
    uint64_t rsp;     // user rsp (rbx)
    uint64_t ss;      // 0x23
} syscall_frame_t;

/**
 * @brief Prefix for linux-compatible syscall traces.
 */
#define linux_syscalls_prefix "Linux syscall: "

/**
 * @brief Wrapper for the dispatcher which gets called for interrupt number 0x80
 * 
 * @param frame 
 */
void int80_handler(InterruptFrame* frame);

/**
 * @brief Wrapper for the dispatcher which gets called for running the syscall instruction
 * 
 * @param f 
 */
void syscall_handler(syscall_frame_t* f);

/**
 * @brief Dispatches syscall based on the syscall number.
 * 
 * @param nr 
 * @param arg1 
 * @param arg2 
 * @param arg3 
 * @param arg4 
 * @param arg5 
 * @param arg6 
 * @return uint64_t 
 */
uint64_t syscall_dispatch (
    uint64_t nr,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5,
    uint64_t arg6
);

#endif
