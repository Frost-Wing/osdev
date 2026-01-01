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
        list_all_mounts();
        return 0;
    }

    const char* device = argv[1];
    const char* mount_point = argv[2];

    general_partition_t* partition = search_general_partition(device);

    if (!partition) {
        printf("mount: %s: partition not found.\n", device);
        return 1;
    }

    void* fs_struct = NULL;
    int ret = 0;

    switch (partition->fs_type) {
        case FS_FAT16:
            fs_struct = kmalloc(sizeof(fat16_fs_t));
            if (!fs_struct) {
                printf("mount: memory allocation failed.");
                return 1;
            }
            ret = fat16_mount(partition->ahci_port, partition->lba_start, (fat16_fs_t*)fs_struct);
            break;

        default:
            printf("mount: unsupported filesystem.");
            return 1;
    }

    if (ret != 0) {
        printf("mount: mounting failed.");
        return 1;
    }

    mount_entry_t* new_mount = add_mount(mount_point, device, partition->fs_type, fs_struct);
    if (!new_mount) {
        printf("mount: add_mount failed.");
        return 1;
    }

    printf("Mounted %s at %s", device, mount_point);
    return 0;
}
