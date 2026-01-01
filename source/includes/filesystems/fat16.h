/**
 * @file fat16.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The header file for reading using FAT16 file system.
 * @version 0.1
 * @date 2025-12-28
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#ifndef FAT16_H
#define FAT16_H

#include <basics.h>
#include <graphics.h>

#define FAT16_EOC 0xFFF8
#define FAT16_ROOT_CLUSTER 0
#define DIR_ENTRIES_PER_SECTOR 16
#define BYTES_PER_DIR_ENTRY 32

typedef struct {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t max_root_dir_entries;
    uint16_t total_sectors_short; // if zero, use total_sectors_long
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;
    // We ignore the rest for now
} __attribute__((packed)) fat16_boot_sector_t;

typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t ignore;       // high word of first cluster (FAT32 only)
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
    uint32_t root_dir_start;
    uint32_t root_dir_sectors;
    uint32_t data_start;

    uint16_t cwd_cluster;    // 0 = root, otherwise cluster number
    char cwd_path[128];
} fat16_fs_t;

typedef struct {
    fat16_fs_t* fs;
    fat16_dir_entry_t entry;
    uint16_t parent_cluster;
    uint32_t pos;
    uint16_t cluster;
} fat16_file_t;

typedef enum {
    FAT_OK = 0,

    // generic
    FAT_ERR_IO,
    FAT_ERR_INVALID,
    FAT_ERR_NOT_FOUND,
    FAT_ERR_EXISTS,
    FAT_ERR_NOT_DIR,
    FAT_ERR_IS_DIR,
    FAT_ERR_NO_SPACE,
    FAT_ERR_NAME_INVALID,
    FAT_ERR_NOT_EMPTY,

    // filesystem corruption (meltdown-worthy)
    FAT_ERR_CORRUPT,
    FAT_ERR_FAT_LOOP,
    FAT_ERR_BAD_CLUSTER,
} fat_err_t;

int detect_fat_type(int8* buf);
int fat16_mount(int portno, uint32_t partition_lba, fat16_fs_t* fs) ;
uint16_t fat16_read_fat_fs(fat16_fs_t* fs, uint16_t cluster);
void fat16_list_root(fat16_fs_t* fs);
int fat16_find_path(fat16_fs_t* fs, const char* path, fat16_dir_entry_t* out);
int fat16_match_name(fat16_dir_entry_t* e, const char* name);
int fat16_find_in_dir(fat16_fs_t* fs, uint16_t current_cluster, const char* name, fat16_dir_entry_t* out);
void fat16_list_dir_cluster(fat16_fs_t* fs, uint16_t start_cluster);
void fat16_format_name(const char* input, char out[11]);
int fat16_find_file(fat16_fs_t* fs, const char* name, fat16_dir_entry_t* out);
void fat16_read_file(fat16_fs_t* fs, fat16_dir_entry_t* file);

int fat16_open(fat16_fs_t* fs, const char* path, fat16_file_t* f);
int fat16_read(fat16_file_t* f, uint8_t* out, uint32_t size);
int fat16_write(fat16_file_t* f, const uint8_t* data, uint32_t size);
void fat16_close(fat16_file_t* f);

uint16_t fat16_find_free_cluster(fat16_fs_t* fs);
void fat16_write_fat_entry(fat16_fs_t* fs, uint16_t cluster, uint16_t value);
uint16_t fat16_allocate_cluster(fat16_fs_t* fs);
uint16_t fat16_append_cluster(fat16_fs_t* fs, uint16_t last_cluster);
void fat16_update_root_entry(fat16_fs_t* fs, fat16_dir_entry_t* entry);
int fat16_update_dir_entry(fat16_fs_t* fs, uint16_t dir_cluster, fat16_dir_entry_t* entry);

int fat16_unlink_path(fat16_fs_t* fs, int16 parent_cluster, cstring name);
int fat16_find_parent(fat16_fs_t* fs, const char* path, uint16_t* out_cluster, char* out_name);
int fat16_delete_entry(fat16_fs_t* fs, uint16_t parent_cluster, const char* name);
int fat16_mkdir(fat16_fs_t* fs, uint16_t parent_cluster, const char* name);
int fat16_create_path(fat16_fs_t* fs, const char* path, uint16_t start_cluster, uint8_t attr);
int fat16_resolve_path(
    fat16_fs_t* fs,
    const char* path,
    uint16_t pwd_cluster,      // current working directory cluster
    uint16_t* out_cluster      // result cluster
);
#endif
