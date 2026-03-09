#ifndef ISO9660_H
#define ISO9660_H

#include <basics.h>
#include <ahci.h>

#define ISO9660_SECTOR_SIZE 2048
#define ISO9660_FLAG_DIR    0x02

typedef struct {
    int portno;
    uint32_t partition_lba;
    uint16_t logical_block_size;
    uint32_t root_extent_lba;
    uint32_t root_size;
} iso9660_fs_t;

typedef struct {
    uint32_t extent_lba;
    uint32_t size;
    uint8_t flags;
} iso9660_dirent_t;

typedef struct {
    iso9660_fs_t* fs;
    iso9660_dirent_t entry;
    uint32_t pos;
} iso9660_file_t;

int iso9660_detect_at_lba(int portno, uint32_t lba_start);
int iso9660_mount(int portno, uint32_t partition_lba, iso9660_fs_t* fs);
int iso9660_find_path(iso9660_fs_t* fs, const char* path, iso9660_dirent_t* out);
int iso9660_open(iso9660_fs_t* fs, const char* path, iso9660_file_t* out);
int iso9660_read(iso9660_file_t* f, uint8_t* out, uint32_t size);
void iso9660_close(iso9660_file_t* f);
int iso9660_list_root(iso9660_fs_t* fs);
int iso9660_list_dir(iso9660_fs_t* fs, const iso9660_dirent_t* dir);

#endif
