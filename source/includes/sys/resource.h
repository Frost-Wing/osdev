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
#include <sys/time.h>

typedef struct {
    uint64_t rlim_cur;
    uint64_t rlim_max;
} linux_rlimit64_t;

typedef struct {
    linux_timeval_t ru_utime;
    linux_timeval_t ru_stime;
    long ru_maxrss;
    long ru_ixrss;
    long ru_idrss;
    long ru_isrss;
    long ru_minflt;
    long ru_majflt;
    long ru_nswap;
    long ru_inblock;
    long ru_oublock;
    long ru_msgsnd;
    long ru_msgrcv;
    long ru_nsignals;
    long ru_nvcsw;
    long ru_nivcsw;
} linux_rusage_t;

_Static_assert(sizeof(linux_rusage_t) == 144, "Linux x86_64 rusage ABI size");

#define LINUX_RUSAGE_SELF     0
#define LINUX_RUSAGE_CHILDREN (-1)
#define LINUX_RUSAGE_THREAD   1

#define LINUX_RLIMIT_CPU        0
#define LINUX_RLIMIT_FSIZE      1
#define LINUX_RLIMIT_DATA       2
#define LINUX_RLIMIT_STACK      3
#define LINUX_RLIMIT_CORE       4
#define LINUX_RLIMIT_RSS        5
#define LINUX_RLIMIT_NPROC      6
#define LINUX_RLIMIT_NOFILE     7
#define LINUX_RLIMIT_MEMLOCK    8
#define LINUX_RLIMIT_AS         9
#define LINUX_RLIMIT_LOCKS      10
#define LINUX_RLIMIT_SIGPENDING 11
#define LINUX_RLIMIT_MSGQUEUE   12
#define LINUX_RLIMIT_NICE       13
#define LINUX_RLIMIT_RTPRIO     14
#define LINUX_RLIMIT_RTTIME     15
#define LINUX_RLIM_INFINITY     UINT64_MAX

#endif