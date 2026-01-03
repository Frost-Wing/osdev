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

typedef enum {
    STDIN  = 0,
    STDOUT = 1,
    STDERR = 2
} stream_t;

void stream_init(void);

/**
 * @brief Redirect output to a file.
 * 
 * @param s the stream type.
 * @param file If file is 'null' then output will be in terminal.
 */
// void stream_set_file(stream_t s, vfs_file_t* file);


void stream_write(stream_t s, const char* buf, size_t len);


void stream_putc(stream_t s, char c);

#endif