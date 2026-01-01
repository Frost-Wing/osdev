/**
 * @file vfs.h
 * @author Pradosh
 * @brief Frostwing Virtual File System (VFS) header
 * @version 0.1
 * @date 2025-12-31
 * 
 * @copyright Copyright (c) Pradosh 2025
 */

#ifndef VFS_H
#define VFS_H

#include <basics.h>
#include <stdint.h>
#include <ahci.h>
#include <filesystems/fat16.h>

typedef struct {
    mount_entry_t* mnt;
    fat16_file_t f;
} vfs_file_t;

typedef struct {
    mount_entry_t* mnt;
    const char*    rel_path;
} vfs_mount_res_t;


// Current working directory
extern char vfs_cwd[256];

/**
 * @brief Open a file at a given path
 * @param path Full path to file
 * @param out_file Pointer to vfs_file_t to receive file handle
 * @return 0 on success, negative on error
 */
int vfs_open(const char* path, vfs_file_t* out_file);

/**
 * @brief Read from an open file
 * @param file Pointer to open file
 * @param buf Buffer to store read data
 * @param size Number of bytes to read
 * @return Number of bytes read or negative on error
 */
int vfs_read(vfs_file_t* file, uint8_t* buf, uint32_t size);

/**
 * @brief Write to an open file
 * @param file Pointer to open file
 * @param buf Buffer containing data
 * @param size Number of bytes to write
 * @return Number of bytes written or negative on error
 */
int vfs_write(vfs_file_t* file, const uint8_t* buf, uint32_t size);

/**
 * @brief Close an open file
 *
 * @param file Pointer to file
 */
void vfs_close(vfs_file_t* file);

/**
 * @brief List files and directories at path
 *
 * @param path Path to directory
 * @return 0 on success, negative on error
 */
int vfs_ls(const char* path);

/**
 * @brief Create a directory at path
 *
 * @param path Full path to directory
 * @return 0 on success, negative on error
 */
int vfs_mkdir(const char* path);

/**
 * @brief Remove a directory at path
 *
 * @param path Full path to directory
 * @return 0 on success, negative on error
 */
int vfs_rmdir(const char* path);

/**
 * @brief Delete a file at path
 *
 * @param path Full path to file
 * @return 0 on success, negative on error
 */
int vfs_unlink(const char* path);

/**
 * @brief Change the current working directory
 *
 * @param path Path to change to (absolute or relative)
 * @return 0 on success, negative on error
 */
int vfs_cd(const char* path);

/**
 * @brief 
 * 
 * @param path 
 * @return int 
 */
int vfs_unlink(const char* path);

/**
 * @brief Get the current working directory
 * @return Pointer to CWD string
 */
const char* vfs_getcwd();

#endif // VFS_H
