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

void sysfs_init(void);

int sysfs_open(vfs_file_t *file);
int sysfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size);
int sysfs_getdent(const char *path, uint64_t index, const char **out_name, procfs_type_t *out_type);
int sysfs_is_dir(const char *path);
int sysfs_ls(const char *path);
#endif