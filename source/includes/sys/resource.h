/**
 * @file resource.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-04-03
 *
 * @copyright Copyright (c) Pradosh 2026
 *
 */
#ifndef SYS_RESOURCE_H
#define SYS_RESOURCE_H

#include <stdint.h>

typedef struct {
    uint64_t rlim_cur;
    uint64_t rlim_max;
} linux_rlimit64_t;

#define LINUX_RLIMIT_CPU 0
#define LINUX_RLIMIT_FSIZE 1
#define LINUX_RLIMIT_DATA 2
#define LINUX_RLIMIT_STACK 3
#define LINUX_RLIMIT_CORE 4
#define LINUX_RLIMIT_RSS 5
#define LINUX_RLIMIT_NPROC 6
#define LINUX_RLIMIT_NOFILE 7
#define LINUX_RLIMIT_MEMLOCK 8
#define LINUX_RLIMIT_AS 9
#define LINUX_RLIMIT_LOCKS 10
#define LINUX_RLIMIT_SIGPENDING 11
#define LINUX_RLIMIT_MSGQUEUE 12
#define LINUX_RLIMIT_NICE 13
#define LINUX_RLIMIT_RTPRIO 14
#define LINUX_RLIMIT_RTTIME 15
#define LINUX_RLIM_INFINITY UINT64_MAX

#endif