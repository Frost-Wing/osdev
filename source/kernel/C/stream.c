/**
 * @file stream.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Unix like standard streams.
 * @version 0.1
 * @date 2026-01-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */

#include <stream.h>
#include <graphics.h>   // flanterm
#include <basics.h>
#include <filesystems/vfs.h>

extern struct flanterm_context* ft_ctx;

typedef struct {
    int index;
    vfs_file_t* file;   // NULL → terminal
} stream_impl_t;

stream_impl_t streams[256];
int next_fd_index = 0;

void stream_init(void)
{
    streams[STDIN].file  = NULL;
    streams[STDIN].index = STDIN;
    next_fd_index++;

    streams[STDOUT].file = NULL;
    streams[STDOUT].index = STDOUT;
    next_fd_index++;

    streams[STDERR].file = NULL;
    streams[STDERR].index = STDERR;
    next_fd_index++;
}

int stream_set_file(stream_t s, vfs_file_t* file)
{
    streams[s].file = file;
    streams[s].index = next_fd_index;
    int last_fd = next_fd_index;
    next_fd_index++;

    return last_fd;
}

void stream_write(stream_t s, const char* buf, size_t len)
{
    if (!buf || len == 0) return;

    stream_impl_t* st = &streams[s];

    if (st->file) {
        vfs_write(st->file, (const uint8_t*)buf, len);
    } else {
        flanterm_write(ft_ctx, buf, len);
    }
}

void stream_putc(stream_t s, char c)
{
    char str[2];
    str[0] = c;
    str[1] = '\0';
    stream_write(s, str, 1);
}
