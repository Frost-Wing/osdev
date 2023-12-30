/**
 * @file tarball.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-12-30
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>

#define block_size 512

struct tarball_header {
    char name[100];
    char mode[8];
    char owner[8];
    char group[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char owner_name[32];
    char group_name[32];
    char dev_major[8];
    char dev_minor[8];
    char prefix[155];
};

void extract_tarball(int64* tarball_addr);