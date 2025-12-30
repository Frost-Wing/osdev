/**
 * @file mbr.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The main code to read the LBA0 and check whether the disk is MBR. 
 * @version 0.1
 * @date 2025-12-28
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <disk/mbr.h>
#include <ahci.h>
#include <filesystems/fat16.h>

mbr_disk_t mbr_disks[10];
int mbr_disks_count = 0;

int check_mbr(int portno){
    uint8_t* buf = kmalloc_aligned(512, 512);
    
    if (!buf) {
        error("[AHCI/MBR] kmalloc failed", __FILE__);
        return -1;
    }
    memset(buf, 0, 512);
    
    if (ahci_read_sector(portno, 0, buf, 1) != 0) {
        error("[AHCI/MBR] Read LBA failed", __FILE__);
        printf("[AHCI/MBR] Read LBA failed on port %d", portno);
        return -2;
    }

    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        info("Invalid MBR signature", __FILE__);
        return -3;
    }

    info("Valid MBR signature", __FILE__);
    parse_mbr_partitions(buf, portno);
    return 0;
}

void parse_mbr_partitions(int8* mbr, int portno){
    mbr_partition_t* partitions = (mbr_partition_t*)&mbr[446];

    mbr_disks[mbr_disks_count].port = portno;
    mbr_disks[mbr_disks_count].sectors = ahci_disks[portno].total_sectors;

    for (int i = 0; i < 4; i++) {
        if (partitions[i].partition_type == 0)
            continue;

        printf("MBR Partition %d: type=0x%X, LBA start=%u, sectors=%u, bootable=%d", i, partitions[i].partition_type, partitions[i].lba_start, partitions[i].num_sectors, partitions[i].boot_flag == 0x80 ? 1 : 0);
        
        mbr_disks[mbr_disks_count].partitions[i].disk = i;
        mbr_disks[mbr_disks_count].partitions[i].lba_start = partitions[i].lba_start;
        mbr_disks[mbr_disks_count].partitions[i].sectors = partitions[i].num_sectors;
        mbr_disks[mbr_disks_count].partitions[i].type = partitions[i].partition_type;

        uint8_t buf[512]; // FAT boot sector is one sector (usually 512 bytes)
        if (ahci_read_sector(portno, partitions[i].lba_start, buf, 1) != 0) {
            error("Failed to read boot sector", __FILE__);
            return;
        }

        int fat_type = detect_fat_type(buf);
        printf("Detected FAT type: %d", fat_type);

        if(fat_type == 16){
            fat16_fs_t fs;
            fat16_dir_entry_t file;

            fat16_mount(portno, partitions[i].lba_start, &fs);
            fat16_list_root(&fs);

            // fat16_file_t f;

            // if (fat16_open(&fs, "/HELLO.TXT", &f) == 0) {
            //     const char* msg = "HELLO FROM FROSTWING\n";
            //     fat16_write(&f, (const uint8_t*)msg, strlen(msg));
            //     fat16_close(&f);
            // }
        }
    }

    mbr_disks_count++;
}