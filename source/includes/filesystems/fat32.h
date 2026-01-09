/**
 * @file fat32.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The implementation of FAT32 FS for FW
 * @version 0.1
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef FAT32_H
#define FAT32_H
#include <basics.h>
#include <filesystems/fat.h>

/* ============================= */
/*   FAT32 CONSTANTS & MACROS    */
/* ============================= */

#define FAT32_SECTOR_SIZE        512
#define FAT32_MAX_LFN_CHARS      255

/* FAT entry values */
#define FAT32_CLUSTER_FREE       0x00000000
#define FAT32_CLUSTER_BAD        0x0FFFFFF7
#define FAT32_CLUSTER_EOC        0x0FFFFFF8

/* Directory entry attributes */
#define FAT_ATTR_READ_ONLY       0x01
#define FAT_ATTR_HIDDEN          0x02
#define FAT_ATTR_SYSTEM          0x04
#define FAT_ATTR_VOLUME_ID       0x08
#define FAT_ATTR_DIRECTORY       0x10
#define FAT_ATTR_ARCHIVE         0x20
#define FAT_ATTR_LFN             0x0F

/* ============================= */
/*   BIOS PARAMETER BLOCK (BPB)  */
/* ============================= */

typedef struct __attribute__((packed)) {
    uint8_t  jump[3];
    uint8_t  oem_name[8];

    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;   /* Must be 0 for FAT32 */
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;        /* Must be 0 for FAT32 */
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    /* FAT32 extended BPB */
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];

    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} fat32_bpb_t;

/* ============================= */
/*   FAT32 DIRECTORY ENTRY       */
/* ============================= */

typedef struct __attribute__((packed)) {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat32_dir_entry_t;

/* ============================= */
/*   LONG FILE NAME ENTRY        */
/* ============================= */

typedef struct __attribute__((packed)) {
    uint8_t  order;
    uint16_t name1[5];
    uint8_t  attr;        /* Always 0x0F */
    uint8_t  type;        /* Always 0 */
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t zero;        /* Always 0 */
    uint16_t name3[2];
} fat32_lfn_entry_t;

/* ============================= */
/*   FAT32 FS CONTEXT            */
/* ============================= */

typedef struct {
    fat32_bpb_t bpb;

    int portno;
    uint32_t partition_lba;

    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t sectors_per_cluster;

    uint32_t total_clusters;
    uint32_t root_cluster;
} fat32_fs_t;

/* ============================= */
/*   FAT32 FILE HANDLE           */
/* ============================= */

typedef struct {
    fat32_fs_t* fs;      // mounted FS
    fat32_dir_entry_t entry;
    uint32_t start_cluster;
    uint32_t current_cluster;
    uint32_t pos;
    uint32_t size;

    uint32_t parent_cluster;
    uint8_t is_dir;
} fat32_file_t;

int fat32_find_path(fat32_fs_t* fs, const char* path, fat32_dir_entry_t* out);
void fat32_list_root(fat32_fs_t* fs);
void fat32_list_dir_cluster(fat32_fs_t* fs, uint32_t cluster);

int fat32_create_path(fat32_fs_t* fs, const char* path, fat32_dir_entry_t* out);
int fat32_truncate(fat32_file_t* file, uint32_t new_size);

uint32_t fat32_alloc_cluster(fat32_fs_t* fs);
const char* fat_next_path_component(const char* path, char* out);

void fat32_free_chain_from(
    fat32_fs_t* fs,
    uint32_t start,
    uint32_t keep);

void fat32_extend_chain(
    fat32_fs_t* fs,
    uint32_t start,
    uint32_t count);

uint32_t fat32_clusters_for_size(
    fat32_fs_t* fs,
    uint32_t size);

void fat32_update_entry(
    fat32_fs_t* fs,
    fat32_dir_entry_t* entry);

int fat32_create_entry(
    fat32_fs_t* fs,
    uint32_t dir_cluster,
    const char* name,
    uint8_t attr,
    fat32_dir_entry_t* out);

int fat32_unlink_path(fat32_fs_t* fs, const char* path);
int fat32_rm_recursive(fat32_fs_t* fs, const char* path);
int fat32_mv(fat32_fs_t* fs, const char* src, const char* dst);
int fat32_delete_entry(fat32_fs_t* fs, uint32_t dir_cluster, const char* name);
int fat32_rmdir(fat32_fs_t* fs, fat32_dir_entry_t* entry);

#endif
