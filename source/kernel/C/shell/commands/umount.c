/**
 * @file umount.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-01-06
 *
 * @copyright Copyright (c) Pradosh 2026
 *
 */

#include <ahci.h>
#include <commands/commands.h>
#include <strings.h>

int cmd_umount(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: umount <mount_point>");
        return 1;
    }

    return vfs_umount(argv[1], false);
}