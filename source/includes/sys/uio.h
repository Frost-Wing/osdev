/**
 * @file uio.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef SYS_UIO_H
#define SYS_UIO_H

#include <stdint.h>

/**
 * @brief Scatter/gather I/O vector.
 *
 * Describes a memory buffer for vectored I/O operations.
 * Used by readv/writev syscalls to perform I/O on multiple buffers.
 */
typedef struct {
    uint64_t iov_base;
    uint64_t iov_len;
} linux_iovec_t;

#endif