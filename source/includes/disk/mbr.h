/**
 * @file mbr.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief  The header to read the LBA0 and check whether the disk is MBR. 
 * @version 0.1
 * @date 2025-12-28
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */
#ifndef MBR_H
#define MBR_H
#include <basics.h>
#include <ahci.h>

#define MBR_PART_BOOTABLE   0x80

extern block_disk_t mbr_disks[10];

typedef struct {
    uint8_t boot_flag;
    uint8_t chs_start[3];
    uint8_t partition_type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t num_sectors;
} __attribute__((packed)) mbr_partition_t;

void check_mbr(int portno);
void parse_mbr_partitions(int8* mbr, int portno);

#endif