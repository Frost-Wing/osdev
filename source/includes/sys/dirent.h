/**
 * @file dirent.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Directory entry structure (64-bit variant).
 * @version 0.1
 * @date 2026-04-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef SYS_DIRENT_H
#define SYS_DIRENT_H

#include <stdint.h>

/**
 * @brief Directory entry structure (64-bit variant).
 *
 * Represents a single entry returned when reading directories.
 * Contains inode number, offset, record length, type, and name.
 */
typedef struct {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
} linux_dirent64_t;

#endif