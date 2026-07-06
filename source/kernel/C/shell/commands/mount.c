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

#include <ahci.h>
#include <commands/commands.h>
#include <filesystems/vfs.h>
#include <heap.h>
#include <strings.h>

int cmd_mount(int argc, char **argv) {
    if (argc == 1) {
        list_all_mounts();
        return 0;
    }

    if (argc < 3) {
        printf("Usage: mount <device> <mount_point>");
        return -1;
    }

    return vfs_mount(argv[1], argv[2], false);
}