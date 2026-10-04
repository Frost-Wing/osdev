/**
 * @file fat16.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The header file for reading using FAT16 file system.
 * @version 0.1
 * @date 2025-12-28
 *
 * @copyright Copyright (c) Pradosh 2025-2026
 *
 */

#ifndef FAT16_H
#define FAT16_H

#include <ahci.h>
#include <basics.h>
#include <filesystems/fat.h>
#include <graphics.h>

#define FAT16_EOC 0xFFF8
#define FAT16_ROOT_CLUSTER 0
#define DIR_ENTRIES_PER_SECTOR 16
#define BYTES_PER_DIR_ENTRY 32
#define FAT16_MAX_CLUSTERS 65525

typedef struct {
    uint8_t jmp[3];
    uint8_t oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t max_root_dir_entries;
    uint16_t total_sectors_short; // if zero, use total_sectors_long
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;
    // We ignore the rest for now
} __attribute__((packed)) fat16_boot_sector_t;

typedef struct {
    char name[8];
    char ext[3];
    uint8_t attr;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t ignore; // high word of first cluster (FAT32 only)
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t first_cluster;
    uint32_t filesize;
} __attribute__((packed)) fat16_dir_entry_t;

typedef struct {
    int portno;
    uint32_t partition_lba;

    fat16_boot_sector_t bs;

    uint32_t fat_start;
    uint32_t cluster_count;
    uint32_t root_dir_start;
    uint32_t root_dir_sectors;
    uint32_t data_start;

    uint16_t cwd_cluster; // 0 = root, otherwise cluster number
    char cwd_path[128];
} fat16_fs_t;

typedef struct {
    fat16_fs_t *fs;
    fat16_dir_entry_t entry;
    uint16_t parent_cluster;
    uint32_t pos;
    uint16_t cluster;
} fat16_file_t;

typedef int (*fat16_cluster_cb)(
    fat16_fs_t *fs,
    uint16_t cluster,
    uint32_t lba,
    void *user);

/** @brief Determine the FAT filesystem variant represented by a boot sector. */
partition_fs_type_t detect_fat_type_enum(const uint8 *buf);

/** @brief Mount a FAT16 partition. */
int fat16_mount(int portno, uint32_t partition_lba, fat16_fs_t *fs);

/** @brief Read the FAT entry for a cluster. */
uint16_t fat16_read_fat_fs(fat16_fs_t *fs, uint16_t cluster);

/** @brief List entries in the root directory. */
int fat16_list_root(fat16_fs_t *fs);

/** @brief Resolve a path to a directory entry. */
int fat16_find_path(fat16_fs_t *fs, const char *path, fat16_dir_entry_t *out);

/** @brief Compare a directory entry name with a path component. */
int fat16_match_name(fat16_dir_entry_t *e, const char *name);

/** @brief Find a named entry in a directory cluster. */
int fat16_find_in_dir(fat16_fs_t *fs, uint16_t current_cluster, const char *name, fat16_dir_entry_t *out);

/** @brief List entries in the directory stored in a cluster. */
int fat16_list_dir_cluster(fat16_fs_t *fs, uint16_t start_cluster);

/** @brief Convert a path name to the FAT 8.3 name format. */
void fat16_format_name(const char *input, char out[11]);

/** @brief Find a file in the current directory. */
int fat16_find_file(fat16_fs_t *fs, const char *name, fat16_dir_entry_t *out);

/** @brief Open a file by path. */
int fat16_open(fat16_fs_t *fs, const char *path, fat16_file_t *f);

/** @brief Read bytes from an open file. */
int fat16_read(fat16_file_t *f, uint8_t *out, uint32_t size);

/** @brief Write bytes to an open file. */
int fat16_write(fat16_file_t *f, const uint8_t *data, uint32_t size);

/** @brief Close an open file. */
void fat16_close(fat16_file_t *f);

/** @brief Find an unused data cluster. */
uint16_t fat16_find_free_cluster(fat16_fs_t *fs);

/** @brief Set the FAT value for a cluster. */
void fat16_write_fat_entry(fat16_fs_t *fs, uint16_t cluster, uint16_t value);

/** @brief Allocate and initialize a free cluster. */
uint16_t fat16_allocate_cluster(fat16_fs_t *fs);

/** @brief Append a new cluster to a cluster chain. */
uint16_t fat16_append_cluster(fat16_fs_t *fs, uint16_t last_cluster);

/** @brief Update an entry in the root directory. */
void fat16_update_root_entry(fat16_fs_t *fs, fat16_dir_entry_t *entry);

/** @brief Update an entry in a subdirectory. */
int fat16_update_dir_entry(fat16_fs_t *fs, uint16_t dir_cluster, fat16_dir_entry_t *entry);

/** @brief Remove a path relative to its parent directory. */
int fat16_unlink_path(fat16_fs_t *fs, uint16 parent_cluster, cstring name);

/** @brief Resolve a path's parent directory and final name. */
int fat16_find_parent(fat16_fs_t *fs, const char *path, uint16_t *out_cluster, char *out_name);

/** @brief Delete a named entry from a directory. */
int fat16_delete_entry(fat16_fs_t *fs, uint16_t parent_cluster, const char *name);

/** @brief Create a directory entry for a new directory. */
int fat16_mkdir(fat16_fs_t *fs, uint16_t parent_cluster, const char *name);

/** @brief Create a file or directory at a path. */
int fat16_create_path(fat16_fs_t *fs, const char *path, uint16_t start_cluster, uint8_t attr);

/** @brief Resolve a path relative to a working-directory cluster. */
int fat16_resolve_path(
    fat16_fs_t *fs,
    const char *path,
    uint16_t pwd_cluster, // current working directory cluster
    uint16_t *out_cluster // result cluster
);

/** @brief Convert a FAT 8.3 name to a display name. */
void fat16_unformat_name(const fat16_dir_entry_t *e, char *out);

/**
 * @brief Release a FAT16 cluster chain.
 *
 * @param fs FAT16 filesystem.
 * @param start_cluster First cluster in the chain.
 */
void fat16_free_chain(fat16_fs_t *fs, uint16_t start_cluster);

/**
 * @brief Change the size of an open FAT16 file.
 *
 * @param f Open file to resize.
 * @param new_size Requested size in bytes.
 * @return 0 on success, negative on error.
 */
int fat16_truncate(fat16_file_t *f, uint32_t new_size);

/**
 * @brief Remove an empty FAT16 directory.
 *
 * @param fs FAT16 filesystem.
 * @param dir_cluster Cluster of the directory to remove.
 * @return 0 on success, negative on error.
 */
int fat16_rmdir(fat16_fs_t *fs, uint16_t dir_cluster);

/**
 * @brief Flush all pending FAT16 writes to disk.
 *
 * Since FrostWing currently performs synchronous writes,
 * all filesystem metadata and data are already committed.
 *
 * @param fs FAT16 filesystem.
 * @return 0 on success.
 */
int fat16_sync(fat16_fs_t *fs);

/** @brief Unmount a FAT16 filesystem. */
void fat16_unmount(fat16_fs_t *fs);
#endif
