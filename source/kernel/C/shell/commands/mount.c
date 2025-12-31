/**
 * @file mount.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Minimal mount command: mounts FAT16 filesystems only.
 * @version 0.1
 * @date 2025-12-31
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>
#include <strings.h>
#include <filesystems/fat16.h>
#include <ahci.h>

int cmd_mount(int argc, char** argv)
{
    if (argc < 3) {
        printf("Usage: mount <device> <mount_point>");
        return 1;
    }

    const char* device = argv[1];
    const char* mount_point = argv[2];

    general_partition_t* partition = search_general_partition(device);

    if(partition == null){
        printf("mount: %s: can't find.", device);
        return -1;
    }

    fat16_fs_t fs;
    if(partition->fs_type == FS_FAT16){
        fat16_mount(partition->ahci_port, partition->lba_start, &fs/* todo */);
    } else {
        printf("mount: bad block or unknown file system.");

        return -1;
    }

    return 0;
}
