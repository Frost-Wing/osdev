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

#define LINUX_SYS_READ        0
#define LINUX_SYS_WRITE       1
#define LINUX_SYS_OPEN        2
#define LINUX_SYS_CLOSE       3
#define LINUX_SYS_LSEEK       8
#define LINUX_SYS_MMAP        9
#define LINUX_SYS_MUNMAP      11
#define LINUX_SYS_BRK         12
#define LINUX_SYS_IOCTL       16
#define LINUX_SYS_WRITEV      20
#define LINUX_SYS_DUP         32
#define LINUX_SYS_DUP2        33
#define LINUX_SYS_GETPID      39
#define LINUX_SYS_EXECVE      59
#define LINUX_SYS_EXIT        60
#define LINUX_SYS_CHDIR       80
#define LINUX_SYS_UNAME       63
#define LINUX_SYS_GETCWD      79
#define LINUX_SYS_GETUID      102
#define LINUX_SYS_GETGID      104
#define LINUX_SYS_ARCH_PRCTL  158
#define LINUX_SYS_OPENAT      257
#define LINUX_SYS_EXIT_GROUP  231

/**
 * @brief Prefix for the syscalls
 */
#define syscalls_prefix "Syscall Invoked: "

/**
 * @brief Prefix for linux-compatible syscall traces.
 */
#define linux_syscalls_prefix "Linux syscall: "

void invoke_syscall(int64 num);
void syscalls_handler(InterruptFrame* frame);

#endif
