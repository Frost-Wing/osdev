/**
 * @file sys.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2026-09-26
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef SYS_H
#define SYS_H

#include <basics.h>
#include <filesystems/vfs.h>
#include <filesystems/layers/proc.h>

/** @brief Initialize the VFS system-information filesystem. */
void sysfs_init(void);

/** @brief Open a sysfs path. */
int sysfs_open(vfs_file_t *file);

/** @brief Read from an open sysfs entry. */
int sysfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size);

/** @brief Get the name and type of a directory entry by index. */
int sysfs_getdent(const char *path, uint64_t index, const char **out_name, procfs_type_t *out_type);

/** @brief Check whether a sysfs path names a directory. */
int sysfs_is_dir(const char *path);

/** @brief Check whether a sysfs path names a symbolic link. */
int sysfs_is_symlink(const char *path);

/** @brief Read the target of a sysfs symbolic link. */
int sysfs_readlink(const char *path, char *buf, uint32_t bufsiz);

/** @brief List entries in a sysfs directory. */
int sysfs_ls(const char *path);
#endif