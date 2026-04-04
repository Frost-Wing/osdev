/**
 * @file dev.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The dev folder to handle by the VFS.
 * @version 0.1
 * @date 2026-04-04
 */

#ifndef DEV_H
#define DEV_H

#include <basics.h>
#include <filesystems/vfs.h>

void devfs_init(void);
int  devfs_open(vfs_file_t* file);
int  devfs_read(vfs_file_t* file, uint8_t* buf, uint32_t size);
int  devfs_write(vfs_file_t* file, const uint8_t* buf, uint32_t size);
void devfs_close(vfs_file_t* file);
int  devfs_ls(void);

#endif
