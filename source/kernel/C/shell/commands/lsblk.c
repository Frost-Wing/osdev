/**
 * @file lsblk.c
 * @author Pradosh
 * @brief List block devices in tree format
 */

#include <ahci.h>
#include <commands/commands.h>

const char *fs_name(partition_fs_type_t fs) {
    switch (fs) {
        case FS_FAT16:
            return "fat16";
        case FS_FAT32:
            return "fat32";
        case FS_ISO9660:
            return "iso9660";
        case FS_PROC:
            return "proc";
        case FS_DEV:
            return "dev";
        case FS_EXT2:
            return "ext2";
        default:
            return "unknown";
    }
}

static const char *mount_point_for_partition(const char *part_name) {
    for (int i = 0; i < mounted_partition_count; i++) {
        mount_entry_t *mount = &mounted_partitions[i];
        if (strcmp(mount->part_name, part_name) == 0)
            return mount->mount_point;
    }

    return "-";
}

static const char *ro_flag_for_filesystem(partition_fs_type_t fs) {
    switch (fs) {
        case FS_ISO9660:
        case FS_DEV:
            return "1";
        default:
            return "0";
    }
}

/* column widths */
#define COL_NAME 16 /* includes the 2-char tree prefix */
#define COL_SIZE 8
#define COL_TYPE 8
#define COL_FS   11
#define COL_RO   4

static void size_to_str(char *out, size_t n, uint64_t sectors, uint32_t sector_size) {
    uint64_t bytes = sectors * sector_size;

    if (bytes >= (1 GiB))
        snprintf(out, n, "%02uG", (uint32_t)(bytes / (1024ULL * 1024 * 1024)));
    else if (bytes >= (1 MiB))
        snprintf(out, n, "%02uM", (uint32_t)(bytes / (1024ULL * 1024)));
    else if (bytes >= (1 KiB))
        snprintf(out, n, "%02uK", (uint32_t)(bytes / 1024));
    else
        snprintf(out, n, "%02uB", (uint32_t)bytes);
}

/* kept in case other code calls it */
void print_size(uint64_t sectors, uint32_t sector_size) {
    char buf[16];
    size_to_str(buf, sizeof(buf), sectors, sector_size);
    printfnoln("%s", buf);
}

static void pad_to(int used, int width) {
    for (int i = used; i < width; i++)
        printfnoln(" ");
}

/* print text, then pad out to the column width */
static void cell(const char *s, int width) {
    printfnoln("%s", s);
    pad_to((int)strlen(s), width);
}

static const char *device_type_name(block_device_type_t type) {
    switch (type) {
        case BLOCK_DEVICE_AHCI: return "disk";
        case BLOCK_DEVICE_NVME: return "nvme";
        case BLOCK_DEVICE_USB:  return "usb" ;
        default:                return "block";
    }
}

int cmd_lsblk(int argc, char **argv) {
    (void)argc;
    (void)argv;

    cell("Name", COL_NAME);
    cell("Size", COL_SIZE);
    cell("Type", COL_TYPE);
    cell("Filesystem", COL_FS);
    cell("RO", COL_RO);
    printf("Mountpoint");

    for (int dev_id = 0; dev_id < block_device_count; dev_id++) {
        block_device_info_t *dev = &block_devices[dev_id];
        if (!dev->present)
            continue;

        char sz[16];

        /* disk row */
        size_to_str(sz, sizeof(sz), dev->total_sectors, dev->sector_size);
        cell(dev->name, COL_NAME);
        cell(sz, COL_SIZE);
        cell(device_type_name(dev->type), COL_TYPE);
        cell("-", COL_FS);
        cell("-", COL_RO);
        printf("-");

        int part_count = 0;
        for (int i = 0; i < general_partition_count; i++) {
            if (ahci_partitions[i].ahci_port == (uint64)dev_id)
                part_count++;
        }

        int seen = 0;
        for (int i = 0; i < general_partition_count; i++) {
            general_partition_t *p = &ahci_partitions[i];
            if (p->ahci_port != (uint64)dev_id)
                continue;

            seen++;
            printfnoln(seen == part_count ? "└─" : "├─");

            /* the tree prefix is 2 columns wide, so the name gets COL_NAME - 2 */
            cell(p->name, COL_NAME - 2);

            size_to_str(sz, sizeof(sz), (uint64_t)p->sector_count, dev->sector_size);
            cell(sz, COL_SIZE);
            cell("part", COL_TYPE);
            cell(fs_name(p->fs_type), COL_FS);
            cell(ro_flag_for_filesystem(p->fs_type), COL_RO);
            printf("%s", mount_point_for_partition(p->name));
        }
    }

    return 0;
}