/**
 * @file ext2.h
 * @author Pradosh
 * @brief The header file for reading/writing the EXT2 file system.
 * @version 0.1
 * @date 2025-12-31
 *
 * @copyright Copyright (c) Pradosh 2025
 *
 * Implements EXT2 revision 0 and revision 1 (dynamic) on-disk format,
 * compatible with the Linux ext2 driver. Designed to be used as a
 * rootfs-capable filesystem in FrostWing.
 */

#ifndef EXT2_H
#define EXT2_H

#include <basics.h>
#include <graphics.h>
#include <ahci.h>
#include <filesystems/fat.h> /* for partition_fs_type_t, FAT_OK/FAT_ERR_* style codes */

/* ===================== On-disk constants ===================== */

#define EXT2_SUPER_MAGIC        0xEF53
#define EXT2_SUPERBLOCK_OFFSET  1024U      /* bytes from partition start */

#define EXT2_GOOD_OLD_REV       0
#define EXT2_DYNAMIC_REV        1

#define EXT2_GOOD_OLD_INODE_SIZE 128

#define EXT2_ROOT_INO           2
#define EXT2_BAD_INO            1
#define EXT2_FIRST_INO          11

#define EXT2_NDIR_BLOCKS        12
#define EXT2_IND_BLOCK          12
#define EXT2_DIND_BLOCK         13
#define EXT2_TIND_BLOCK         14
#define EXT2_N_BLOCKS           15

#define EXT2_NAME_LEN           255

/* i_mode values */
#define EXT2_S_IFMT   0xF000
#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK  0xA000
#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFBLK  0x6000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFCHR  0x2000
#define EXT2_S_IFIFO  0x1000

#define EXT2_S_IRWXU  0x01C0
#define EXT2_S_IRUSR  0x0100
#define EXT2_S_IWUSR  0x0080
#define EXT2_S_IXUSR  0x0040
#define EXT2_S_IRWXG  0x0038
#define EXT2_S_IRWXO  0x0007

/* directory entry file types (filetype feature) */
#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

/* feature flags we understand */
#define EXT2_FEATURE_INCOMPAT_FILETYPE   0x0002
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001

/* error codes (kept consistent with FAT driver's style) */
#define EXT2_OK               0
#define EXT2_ERR_NOT_FOUND   -1
#define EXT2_ERR_CORRUPT     -2
#define EXT2_ERR_NOSPACE     -3
#define EXT2_ERR_IO          -4
#define EXT2_ERR_EXISTS      -5
#define EXT2_ERR_NOTDIR      -6
#define EXT2_ERR_ISDIR       -7
#define EXT2_ERR_INVAL       -8
#define EXT2_ERR_NOTEMPTY    -9

/* ===================== On-disk structures ===================== */

typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;

    /* EXT2_DYNAMIC_REV fields */
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;

    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;

    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;

    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;

    uint8_t  s_unused[760];
} __attribute__((packed)) ext2_superblock_t;

typedef struct {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t  bg_reserved[12];
} __attribute__((packed)) ext2_group_desc_t;

typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;       /* low 32 bits of size */
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;     /* 512-byte sectors used */
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[EXT2_N_BLOCKS];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_size_high;  /* dir_acl in old rev */
    uint32_t i_faddr;
    uint8_t  i_osd2[12];
} __attribute__((packed)) ext2_inode_t;

typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[EXT2_NAME_LEN];
} ext2_dir_entry_t; /* in-memory unpacked form; on-disk name is variable length */

#define EXT2_DIR_ENTRY_FIXED_SIZE 8 /* inode + rec_len + name_len + file_type, on disk */

/* ===================== Runtime structures ===================== */

typedef struct {
    int      portno;
    uint32_t partition_lba;     /* LBA of start of partition, sector units */

    ext2_superblock_t sb;

    uint32_t block_size;        /* in bytes */
    uint32_t sectors_per_block; /* block_size / 512 */
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t inode_size;
    uint32_t groups_count;
    uint32_t gdt_block;         /* block number where group desc table starts */
    int      has_filetype;

    uint32_t cwd_ino;           /* 0 = not set -> root */
    char     cwd_path[128];

    int      sb_dirty;
} ext2_fs_t;

typedef struct {
    ext2_fs_t* fs;
    uint32_t   ino;
    ext2_inode_t inode;
    uint32_t   pos;
    uint16_t   is_dir;
} ext2_file_t;

/* ===================== API ===================== */

partition_fs_type_t detect_ext2_type_enum(int portno, uint32_t partition_lba);

int ext2_mount(int portno, uint32_t partition_lba, ext2_fs_t* fs);
void ext2_unmount(ext2_fs_t* fs);

int ext2_find_path(ext2_fs_t* fs, const char* path, uint32_t* out_ino, ext2_inode_t* out_inode);
int ext2_list_dir(ext2_fs_t* fs, uint32_t dir_ino);

int ext2_open(ext2_fs_t* fs, const char* path, ext2_file_t* f);
int ext2_create(ext2_fs_t* fs, const char* path, uint16_t mode, ext2_file_t* f);
int ext2_read(ext2_file_t* f, uint8_t* out, uint32_t size);
int ext2_write(ext2_file_t* f, const uint8_t* data, uint32_t size);
void ext2_close(ext2_file_t* f);

int ext2_mkdir(ext2_fs_t* fs, const char* path);
int ext2_unlink_path(ext2_fs_t* fs, const char* path);
int ext2_rmdir(ext2_fs_t* fs, const char* path);
int ext2_rm_recursive(ext2_fs_t* fs, const char* path);
int ext2_rename(ext2_fs_t* fs, const char* src_path, const char* dst_path);
int ext2_truncate(ext2_fs_t* fs, ext2_file_t* f);

int ext2_find_in_dir(ext2_fs_t* fs, uint32_t dir_ino, const char* name, uint32_t* out_ino, uint8_t* out_type);

#endif /* EXT2_H */
