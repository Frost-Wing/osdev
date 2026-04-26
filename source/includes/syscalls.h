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
#define LINUX_SYS_LSTAT             6
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
#define LINUX_SYS_SOCKET            41
#define LINUX_SYS_CONNECT           42
#define LINUX_SYS_CLONE             56
#define LINUX_SYS_FORK              57
#define LINUX_SYS_WAIT4             61
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

#define LINUX_AT_FDCWD   (-100)

#define LINUX_O_RDONLY   0x0000
#define LINUX_O_WRONLY   0x0001
#define LINUX_O_RDWR     0x0002
#define LINUX_O_CREAT    0x0040
#define LINUX_O_TRUNC    0x0200
#define LINUX_O_APPEND   0x0400

#define LINUX_SEEK_SET   0
#define LINUX_SEEK_CUR   1
#define LINUX_SEEK_END   2

#define LINUX_EBADF      9
#define LINUX_EFAULT     14
#define LINUX_EACCES     13
#define LINUX_EINVAL     22
#define LINUX_ENOEXEC    8
#define LINUX_ENOTTY     25
#define LINUX_ENOSYS     38
#define LINUX_ENFILE     23
#define LINUX_ENOENT     2
#define LINUX_ENOMEM     12
#define LINUX_ERANGE     34
#define LINUX_ENOTDIR    20
#define LINUX_ESRCH      3
#define LINUX_EPERM      1
#define LINUX_EINTR      4
#define LINUX_ETIMEDOUT  110
#define LINUX_ECHILD     10
#define LINUX_ENOTSOCK   88
#define LINUX_EAFNOSUPPORT 97

#define LINUX_PROT_NONE  0x0
#define LINUX_PROT_READ  0x1
#define LINUX_PROT_WRITE 0x2
#define LINUX_PROT_EXEC  0x4

#define LINUX_MAP_SHARED    0x01
#define LINUX_MAP_PRIVATE   0x02
#define LINUX_MAP_FIXED     0x10
#define LINUX_MAP_ANONYMOUS 0x20

#define LINUX_S_IFMT     00170000
#define LINUX_S_IFDIR    0040000
#define LINUX_S_IFREG    0100000
#define LINUX_S_IFCHR    0020000

#define LINUX_AT_SYMLINK_NOFOLLOW 0x100
#define LINUX_AT_EMPTY_PATH       0x1000

#define LINUX_F_DUPFD    0
#define LINUX_F_GETFD    1
#define LINUX_F_SETFD    2
#define LINUX_F_GETFL    3
#define LINUX_F_SETFL    4

#define LINUX_TIOCGWINSZ 0x5413
#define LINUX_TCGETS     0x5401
#define LINUX_TCSETS     0x5402
#define LINUX_TCSETSW    0x5403
#define LINUX_TCSETSF    0x5404
#define LINUX_TIOCGPGRP  0x540F
#define LINUX_TIOCSPGRP  0x5410

#define LINUX_ARCH_SET_FS 0x1002
#define LINUX_ARCH_GET_FS 0x1003

#define LINUX_CLOCK_REALTIME 0
#define LINUX_CLOCK_MONOTONIC 1

#define IA32_FS_BASE_MSR 0xC0000100

#define PROC_FILE_COUNT 3

#define FW_SYS_GETC      0x1000
#define FW_SYS_GETC_NB   0x1001
#define FW_SYS_PUTC      0x1002
#define FW_SYS_LOGIN     0x1055

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
