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

/** @brief Initialize the VFS device filesystem. */
void devfs_init(void);

/** @brief Open a device node through the VFS. */
int devfs_open(vfs_file_t *file);

/** @brief Read bytes from an open device node. */
int devfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size);

/** @brief Write bytes to an open device node. */
int devfs_write(vfs_file_t *file, const uint8_t *buf, uint32_t size);

/** @brief Close an open device node. */
void devfs_close(vfs_file_t *file);

/** @brief List available device nodes. */
int devfs_ls(void);

#endif
