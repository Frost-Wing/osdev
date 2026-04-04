/**
 * @file stat.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef SYS_STAT_H
#define SYS_STAT_H

#include <stdint.h>
#include <sys/time.h>

/**
 * @brief File status information (legacy stat structure).
 *
 * Contains metadata about a file such as size, permissions,
 * ownership, timestamps, and device identifiers.
 * Returned by stat-family syscalls.
 */
typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    linux_timespec_t st_atim;
    linux_timespec_t st_mtim;
    linux_timespec_t st_ctim;
    int64_t __unused[3];
} linux_stat_t;

/**
 * @brief Extended file status information.
 *
 * Modern replacement for stat, providing more detailed and flexible
 * file metadata, including birth time and attribute masks.
 * Used with the statx syscall.
 */
typedef struct {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t __spare0[1];
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    linux_timespec_t stx_atime;
    linux_timespec_t stx_btime;
    linux_timespec_t stx_ctime;
    linux_timespec_t stx_mtime;
    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t __spare2[14];
} linux_statx_t;

#endif