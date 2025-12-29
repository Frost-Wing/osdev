/**
 * @file lsblk.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief List block devices in tree format
 * @version 0.1
 * @date 2025-10-07
 *
 * @copyright Copyright (c) Pradosh 2025
 */

#include <commands/commands.h>
#include <disk/mbr.h>
#include <ahci.h> 

void print_size(uint64_t sectors)
{
    uint64_t bytes = sectors * 512;

    if (bytes >= (1 GiB)) {
        printfnoln("%uG", (uint32_t)(bytes / (1024ULL * 1024 * 1024)));
    } else if (bytes >= (1 MiB)) {
        printfnoln("%uM", (uint32_t)(bytes / (1024ULL * 1024)));
    } else if (bytes >= (1 KiB)) {
        printfnoln("%uK", (uint32_t)(bytes / 1024));
    } else {
        printfnoln("%uB", (uint32_t)bytes);
    }
}


int cmd_lsblk(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("NAME        SIZE     TYPE");

    for (int i = 0; i < 10; i++) {
        block_disk_t* d = &mbr_disks[i];

        if (d->sectors == 0)
            continue;

        // ---- DISK ----
        printfnoln("disk%d       ", i);
        print_size(d->sectors);
        printf("     disk");

        // count valid partitions
        int part_count = 0;
        for (int p = 0; p < 3; p++) {
            if (d->partitions[p].sectors != 0)
                part_count++;
        }

        // ---- PARTITIONS ----
        int printed = 0;
        for (int p = 0; p < 3; p++) {
            block_part_t* part = &d->partitions[p];

            if (part->sectors == 0)
                continue;

            printed++;

            if (printed == part_count)
                printfnoln("└─");
            else
                printfnoln("├─");

            printfnoln("disk%dp%d   ", i, p + 1);
            print_size(part->sectors);
            printf("     part");
        }
    }

    return 0;
}
