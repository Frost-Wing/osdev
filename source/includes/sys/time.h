/**
 * @file time.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef SYS_TIME_H
#define SYS_TIME_H

/**
 * @brief Represents time with nanosecond precision.
 *
 * Used by various syscalls to express absolute or relative time.
 * Stores seconds and nanoseconds since the Unix epoch or for intervals.
 */
typedef struct {
    long tv_sec;
    long tv_nsec;
} linux_timespec_t;

#endif