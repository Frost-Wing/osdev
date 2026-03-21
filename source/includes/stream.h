/**
 * @file stream.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Unix like standard streams.
 * @version 0.1
 * @date 2026-01-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STDIN  = 0,
    STDOUT = 1,
    STDERR = 2
} stream_t;

// #include <ahci.h>
// #include <filesystems/vfs.h>

#define STREAM_MAX_FDS 256

void stream_init(void);

/**
 * @brief Redirect output to a file.
 * 
 * @param s the stream type.
 * @param file If file is 'null' then output will be in terminal.
 * 
 * @returns File descriptor of the given file.
 */
// int stream_set_file(stream_t s, vfs_file_t* file);
// vfs_file_t* stream_get_file(stream_t s);


void stream_write(stream_t s, const char* buf, size_t len);


void stream_putc(stream_t s, char c);

void fd_table_init(void);
bool fd_valid(int fd);
// vfs_file_t* fd_get_file(int fd);
int fd_open(const char* path, int flags);
int fd_close(int fd);
int fd_dup(int oldfd);
int fd_dup2(int oldfd, int newfd);
int fd_flags(int fd);
uint32_t fd_file_size(int fd);
uint32_t* fd_pos_ptr(int fd);

#endif
