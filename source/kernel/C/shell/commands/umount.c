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

#include <commands/commands.h>
#include <ahci.h>
#include <filesystems/fat16.h>
#include <strings.h>

int cmd_umount(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: umount <mount_point>");
        return 1;
    }

    const char* mount_point = argv[1];

    /* Never allow root unmount */
    if (strcmp(mount_point, "/") == 0)
        printf("umount: warn unmounting root filesystem");

    mount_entry_t* m = find_mount_by_point(mount_point);
    if (!m) {
        printf("umount: %s: not mounted", mount_point);
        return 1;
    }

    /* Filesystem-specific cleanup */
    switch (m->type) {
        case FS_FAT16:
            if (m->fs) {
                fat16_unmount((fat16_fs_t*)m->fs);
                kfree(m->fs);
            }
            break;

        case FS_PROC:
            // procfs_shutdown(); /* or procfs_unmount() */
            break;

        default:
            printf("umount: unsupported filesystem");
            return 1;
    }

    if (remove_mount(mount_point) != 0) {
        printf("umount: failed to remove mount");
        return 1;
    }

    printf("umount: %s unmounted", mount_point);
    return 0;
}