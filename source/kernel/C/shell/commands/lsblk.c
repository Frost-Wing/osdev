/**
 * @file lsblk.c
 * @author Pradosh
 * @brief List block devices in tree format
 */

#include <commands/commands.h>
#include <ahci.h>

void print_size(uint64_t sectors)
{
    uint64_t bytes = sectors * 512;

    if (bytes >= (1 GiB)) {
        printfnoln("%02uG", (uint32_t)(bytes / (1024ULL * 1024 * 1024)));
    } else if (bytes >= (1 MiB)) {
        printfnoln("%02uM", (uint32_t)(bytes / (1024ULL * 1024)));
    } else if (bytes >= (1 KiB)) {
        printfnoln("%02uK", (uint32_t)(bytes / 1024));
    } else {
        printfnoln("%02uB", (uint32_t)bytes);
    }
}

static const char* fs_name(partition_fs_type_t fs)
{
    switch (fs) {
        case FS_FAT16:   return "fat16";
        case FS_FAT32:   return "fat32";
        case FS_ISO9660: return "iso9660";
        case FS_PROC:    return "proc";
        default:         return "unknown";
    }
}

static const char* device_type_name(block_device_type_t type)
{
    switch (type) {
        case BLOCK_DEVICE_AHCI: return "disk";
        case BLOCK_DEVICE_NVME: return "nvme";
        default: return "block";
    }
}

int cmd_lsblk(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("Name           Size     Type     Filesystem");

    for (int dev_id = 0; dev_id < block_device_count; dev_id++) {
        block_device_info_t* dev = &block_devices[dev_id];
        if (!dev->present)
            continue;

        uint64_t disk_sectors = dev->total_sectors;

        printfnoln("%s", dev->name);
        for (int pad = (int)strlen(dev->name); pad < 15; pad++)
            printfnoln(" ");
        print_size(disk_sectors);
        printf("      %s      -", device_type_name(dev->type));

        int part_count = 0;
        for (int i = 0; i < general_partition_count; i++) {
            if (ahci_partitions[i].ahci_port == (int64)dev_id)
                part_count++;
        }

        int seen = 0;
        for (int i = 0; i < general_partition_count; i++) {
            general_partition_t* p = &ahci_partitions[i];
            if (p->ahci_port != (int64)dev_id)
                continue;

            seen++;
            int last = (seen == part_count);

            if (last) printfnoln("└─");
            else      printfnoln("├─");

            printfnoln("%s    ", p->name);
            print_size((uint64_t)p->sector_count);
            printf("      part     %s", fs_name(p->fs_type));
        }
    }

    return 0;
}
