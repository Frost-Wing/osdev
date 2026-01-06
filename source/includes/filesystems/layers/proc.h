/**
 * @file proc.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The proc folder to handle by the VFS.
 * @version 0.1
 * @date 2026-01-05
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */

#ifndef PROC_H
#define PROC_H

#include <basics.h>
#include <filesystems/vfs.h>

typedef int (*procfs_read_cb)(
    vfs_file_t* file,
    uint8_t* buf,
    uint32_t size,
    void* priv
);

typedef int (*procfs_write_cb)(
    vfs_file_t* file,
    const uint8_t* buf,
    uint32_t size,
    void* priv
);

/* One proc file entry */
typedef struct procfs_entry {
    const char* name;          // "stat", "uptime", etc
    procfs_read_cb  read;
    procfs_write_cb write;
    void* priv;
} procfs_entry_t;

/* API */
void procfs_init(void);
int  procfs_register(procfs_entry_t* entry);

int  procfs_open(vfs_file_t* file);
int  procfs_read(vfs_file_t* file, uint8_t* buf, uint32_t size);
int  procfs_write(vfs_file_t* file, const uint8_t* buf, uint32_t size);
void procfs_close(vfs_file_t* file);

#endif
