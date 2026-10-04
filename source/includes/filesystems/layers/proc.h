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
    vfs_file_t *file,
    uint8_t *buf,
    uint32_t size,
    void *priv);

typedef int (*procfs_write_cb)(
    vfs_file_t *file,
    const uint8_t *buf,
    uint32_t size,
    void *priv);

typedef enum {
    PROC_FILE,
    PROC_DIR,
    PROC_SYMLINK
} procfs_type_t;

typedef struct procfs_entry {
    const char *name;
    procfs_type_t type;

    int (*read)(vfs_file_t *, uint8_t *, uint32_t, void *);
    int (*write)(vfs_file_t *, const uint8_t *, uint32_t, void *);

    void *priv;
} procfs_entry_t;

/* API */
/** @brief Initialize the VFS process-information filesystem. */
void procfs_init(void);

/** @brief Register a file or directory in procfs. */
int procfs_register(procfs_entry_t *entry);

/** @brief Open a procfs path. */
int procfs_open(vfs_file_t *file);

/** @brief Read from an open procfs entry. */
int procfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size);

/** @brief Write to an open procfs entry when supported. */
int procfs_write(vfs_file_t *file, const uint8_t *buf, uint32_t size);

/** @brief Close an open procfs entry. */
void procfs_close(vfs_file_t *file);

/** @brief Get the name and type of a directory entry by index. */
int procfs_getdent(const char *path, uint64_t index, const char **out_name, procfs_type_t *out_type);

/** @brief List entries in a procfs directory. */
int procfs_ls(const char *path);

/** @brief Check whether a procfs path names a directory. */
int procfs_path_is_dir(const char *path);

#endif
