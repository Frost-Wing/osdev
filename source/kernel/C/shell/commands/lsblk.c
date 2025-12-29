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
#include <disk/gpt.h>
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

static void print_part_tree(
    int disk_id,
    uint64_t disk_sectors,
    void* parts,
    int part_count,
    int is_gpt
) {
    // ---- DISK ----
    printfnoln("disk%d       ", disk_id);
    print_size(disk_sectors);
    printf("     disk");

    for (int p = 0; p < part_count; p++) {
        int last = (p == part_count - 1);

        if (last)
            printfnoln("└─");
        else
            printfnoln("├─");

        printfnoln("disk%dp%d   ", disk_id, p + 1);

        if (!is_gpt) {
            block_part_t* part = &((block_part_t*)parts)[p];
            print_size(part->sectors);
        } else {
            gpt_partition_t* part = &((gpt_partition_t*)parts)[p];
            print_size(part->sectors);
        }

        printf("     part");
    }
}

int cmd_lsblk(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("NAME        SIZE     TYPE");

    int disk_id = 0;

    /* ---------- MBR DISKS ---------- */
    for (int i = 0; i < mbr_disks_count; i++) {
        block_disk_t* d = &mbr_disks[i];

        if (d->sectors == 0)
            continue;

        // count partitions
        int part_count = 0;
        for (int p = 0; p < 4; p++) {
            if (d->partitions[p].sectors)
                part_count++;
        }

        print_part_tree(
            disk_id++,
            d->sectors,
            d->partitions,
            part_count,
            0   // MBR
        );
    }

    /* ---------- GPT DISKS ---------- */
    for (int i = 0; i < gpt_disks_count; i++) {
        gpt_disk_t* d = &gpt_disks[i];

        if (d->sectors == 0 || d->partition_count == 0)
            continue;

        print_part_tree(
            disk_id++,
            d->sectors,
            d->partitions,
            d->partition_count,
            1   // GPT
        );
    }

    return 0;
}
