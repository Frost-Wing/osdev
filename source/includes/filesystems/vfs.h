/**
 * @file vfs.h
 * @author Pradosh
 * @brief Frostwing Virtual File System (VFS) header
 * @version 0.1
 * @date 2025-12-31
 *
 * @copyright Copyright (c) Pradosh 2025-2026
 */

#ifndef VFS_H
#define VFS_H

#include <filesystems/ext2.h>
#include <filesystems/fat16.h>
#include <filesystems/fat32.h>
#include <filesystems/iso9660.h>
#include <stddef.h>

typedef struct vfs_file {
    mount_entry_t *mnt;

    union {
        fat16_file_t fat16;
        fat32_file_t fat32;
        iso9660_file_t iso9660;
        ext2_file_t ext2;
    } f;

    uint32_t pos; // for virtual files only, must not be used for real fs
    int flags;

    char rel_path[64]; // for virtual files only, must not be used for real fs
} vfs_file_t;

typedef struct {
    mount_entry_t *mnt;
    const char *rel_path;
} vfs_mount_res_t;

/* Open flags */
#define VFS_RDONLY 0x0001
#define VFS_WRONLY 0x0002
#define VFS_RDWR 0x0003

#define VFS_CREATE 0x0100
#define VFS_TRUNC 0x0200
#define VFS_APPEND 0x0400

#define VFS_FILE_ROOT_DIR (1 << 0)

// Current working directory
extern char vfs_cwd[256];

/**
 * @brief Open a file at a given path
 * @param path Full path to file
 * @param out_file Pointer to vfs_file_t to receive file handle
 * @return 0 on success, negative on error
 */
int vfs_open(const char *path, int flags, vfs_file_t *out_file);

/**
 * @brief Read the target of a symbolic link.
 *
 * @param path Path to the symbolic link.
 * @param buf Buffer to receive the link target.
 * @param bufsiz Size of @p buf in bytes.
 * @return Number of bytes written, or a negative error code.
 */
int vfs_readlink(const char *path, char *buf, uint32_t bufsiz);

/**
 * @brief Read from an open file
 * @param file Pointer to open file
 * @param buf Buffer to store read data
 * @param size Number of bytes to read
 * @return Number of bytes read or negative on error
 */
int vfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size);

/**
 * @brief Write to an open file
 * @param file Pointer to open file
 * @param buf Buffer containing data
 * @param size Number of bytes to write
 * @return Number of bytes written or negative on error
 */
int vfs_write(vfs_file_t *file, const uint8_t *buf, uint32_t size);

/**
 * @brief Close an open file
 *
 * @param file Pointer to file
 */
void vfs_close(vfs_file_t *file);

/**
 * @brief Checks whether a given path exists and is a directory,
 *        without changing the current working directory.
 *
 * @param path Path to check (absolute or relative to vfs_cwd)
 * @return 1 if it exists and is a directory, 0 if it doesn't exist
 *         or isn't a directory, negative on error.
 */
int vfs_path_is_dir(const char *path);

/**
 * @brief List files and directories at path
 *
 * @param path Path to directory
 * @return 0 on success, negative on error
 */
int vfs_ls(const char *path);

/**
 * @brief Create a directory at path
 *
 * @param path Full path to directory
 * @return 0 on success, negative on error
 */
int vfs_mkdir(const char *path);

/**
 * @brief Remove a directory at path
 *
 * @param path Full path to directory
 * @return 0 on success, negative on error
 */
int vfs_rmdir(const char *path);

/**
 * @brief Delete a file at path
 *
 * @param path Full path to file
 * @return 0 on success, negative on error
 */
int vfs_unlink(const char *path);

/**
 * @brief Change the current working directory
 *
 * @param path Path to change to (absolute or relative)
 * @return 0 on success, negative on error
 */
int vfs_cd(const char *path);

/**
 * @brief Get the current working directory
 * @return Pointer to CWD string
 */
const char *vfs_getcwd(void);

/**
 * @brief Copy a file from src to dst. Works across different mounted
 *        filesystems (e.g. FAT32 -> EXT2) since it copies via the
 *        generic vfs_read/vfs_write path. If dst is an existing
 *        directory, the file is copied into it under its own name.
 *        Directories are not currently supported.
 *
 * @param src Path to source file
 * @param dst Path to destination file or directory
 * @return 0 on success, negative on error
 */
int vfs_cp(const char *src, const char *dst);

/**
 * @brief Move or rename a file.
 *
 * @param src Source path.
 * @param dst Destination path.
 * @return 0 on success, negative on error.
 */
int vfs_mv(const char *src, const char *dst);

/**
 * @brief Return the final path component.
 *
 * @param path Path to inspect.
 * @return Pointer to the basename within @p path.
 */
const char *vfs_basename(const char *path);

/**
 * @brief Normalize a path using the VFS current working directory.
 *
 * @param in Input path.
 * @param out Buffer to receive the normalized path.
 * @param out_sz Size of @p out in bytes.
 * @return 0 on success, negative on error.
 */
int vfs_normalize_path(const char *in, char *out, size_t out_sz);

/**
 * @brief Resolve a path to its mounted filesystem and mount-relative path.
 *
 * @param path Path to resolve.
 * @param out Receives the mount and relative path.
 * @return 0 on success, negative on error.
 */
int vfs_resolve_mount(const char *path, vfs_mount_res_t *out);

/**
 * @brief Flush pending changes to mounted filesystems.
 *
 * @param kernel_call Whether the call originates from kernel code.
 * @return 0 on success, negative on error.
 */
int vfs_sync(bool kernel_call);

/**
 * @brief Mount a disk at a path.
 *
 * @param diskname Name of the disk device.
 * @param mount_point Path at which to mount the filesystem.
 * @param is_kernel_call Whether the call originates from kernel code.
 * @return 0 on success, negative on error.
 */
int vfs_mount(const char *diskname, const char *mount_point, bool is_kernel_call);

/**
 * @brief Unmount the filesystem mounted at a path.
 *
 * @param mount_point Mount path.
 * @param is_kernel_call Whether the call originates from kernel code.
 * @return 0 on success, negative on error.
 */
int vfs_umount(const char *mount_point, bool is_kernel_call);

/**
 * @brief Unmount all mounted filesystems.
 *
 * @param is_kernel_call Whether the call originates from kernel code.
 * @return 0 on success, negative on error.
 */
int vfs_umount_all(bool is_kernel_call);

/**
 * @brief Recursively remove a directory tree.
 *
 * @param path Path to the directory tree.
 * @return 0 on success, negative on error.
 */
int vfs_rm_recursive(const char *path);

/**
 * @brief Create a file or directory at a path.
 *
 * @param path Path to create.
 * @param attr FAT-style attributes; bit 0x10 requests a directory.
 * @return 0 on success, negative on error.
 */
int vfs_create_path(const char *path, uint8_t attr);

#endif // VFS_H
