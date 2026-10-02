/**
 * @file dev.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The dev folder to handle by the VFS.
 * @version 0.1
 * @date 2026-04-04
 */

#include <ahci.h>
#include <basics.h>
#include <cc-asm.h>
#include <filesystems/layers/dev.h>
#include <graphics.h>
#include <heap.h>
#include <klog.h>
#include <memory.h>
#include <strings.h>
#include <syslog.h>
#include <tty.h>

static uint64_t devfs_rng_state = 0x9E3779B97F4A7C15ULL;

static int devfs_disk_id_from_name(const char *name) {
    if (!name || !*name)
        return -1;

    for (int i = 0; i < block_device_count; i++) {
        block_device_info_t *dev = &block_devices[i];
        if (!dev->present)
            continue;

        if (strcmp(dev->name, name) == 0)
            return i;
    }

    return -1;
}

static bool devfs_is_block_node(const char *name) {
    int id = devfs_disk_id_from_name(name);
    if (id >= 0)
        return true;

    for (int i = 0; i < general_partition_count; i++) {
        int device_id = (int)ahci_partitions[i].ahci_port;
        if (strcmp(ahci_partitions[i].name, name) == 0 &&
            block_get_device(device_id))
            return true;
    }
    return false;
}

void devfs_init(void) {
    devfs_rng_state ^= rdtsc64();
}

static uint8_t devfs_next_rand_u8(void) {
    devfs_rng_state ^= devfs_rng_state << 13;
    devfs_rng_state ^= devfs_rng_state >> 7;
    devfs_rng_state ^= devfs_rng_state << 17;
    return (uint8_t)(devfs_rng_state & 0xFF);
}

int devfs_open(vfs_file_t *file) {
    if (!file || !file->rel_path)
        return -1;

    if (file->rel_path[0] == '\0') {
        file->pos = 0;
        return 0;
    }

    if (strcmp(file->rel_path, "null") == 0 ||
        strcmp(file->rel_path, "zero") == 0 ||
        strcmp(file->rel_path, "random") == 0 ||
        strcmp(file->rel_path, "urandom") == 0 ||
        strcmp(file->rel_path, "klog") == 0 ||
        strcmp(file->rel_path, "syslog") == 0 ||
        strcmp(file->rel_path, "tty") == 0 ||
        strcmp(file->rel_path, "tty1") == 0 ||
        strcmp(file->rel_path, "rtc") == 0 ||
        strcmp(file->rel_path, "rtc0") == 0) {

        file->pos = 0;
        return 0;
    }

    if (devfs_is_block_node(file->rel_path)) {
        file->pos = 0;
        return 0;
    }

    return -1;
}

int devfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size) {
    if (!file || !buf)
        return -1;

    if (strcmp(file->rel_path, "null") == 0)
        return 0;

    if (strcmp(file->rel_path, "rtc") == 0 ||
        strcmp(file->rel_path, "rtc0") == 0)
        return 0;

    if (strcmp(file->rel_path, "tty") == 0 ||
        strcmp(file->rel_path, "tty1") == 0)
        return tty_read((char *)buf, size);

    if (strcmp(file->rel_path, "zero") == 0) {
        memset(buf, 0, size);
        file->pos += size;
        return (int)size;
    }

    if (strcmp(file->rel_path, "random") == 0 || strcmp(file->rel_path, "urandom") == 0) {
        for (uint32_t i = 0; i < size; i++)
            buf[i] = devfs_next_rand_u8();
        file->pos += size;
        return (int)size;
    }

    if (strcmp(file->rel_path, "klog") == 0) {
        size_t n = klog_read_at((char *)buf, (size_t)file->pos, size);
        file->pos += n;
        return (int)n;
    }

    if (strcmp(file->rel_path, "syslog") == 0) {
        size_t n = syslog_read_at((char *)buf, (size_t)file->pos, size);
        file->pos += n;
        return (int)n;
    }

    int disk_id = devfs_disk_id_from_name(file->rel_path);
    uint64_t partition_start = 0;
    uint64_t total_sectors = 0;
    bool is_partition = false;
    if (disk_id < 0) {
        for (int i = 0; i < general_partition_count; i++) {
            general_partition_t *part = &ahci_partitions[i];
            if (strcmp(part->name, file->rel_path) != 0)
                continue;
            disk_id = (int)part->ahci_port;
            partition_start = part->lba_start;
            total_sectors = part->sector_count;
            is_partition = true;
            break;
        }
    }
    if (disk_id < 0)
        return -1;

    block_device_info_t *dev = block_get_device(disk_id);
    if (!dev || dev->sector_size == 0)
        return -1;
    if (dev->type == BLOCK_DEVICE_USB && dev->total_sectors == 0 &&
        usb_msc_refresh_device(dev->backend_index) != 0)
        return -1;
    dev = block_get_device(disk_id);
    if (!dev)
        return -1;
    if (total_sectors == 0)
        total_sectors = dev->total_sectors;
    if (total_sectors > UINT64_MAX / dev->sector_size)
        return -1;
    uint64_t device_size = total_sectors * dev->sector_size;
    if (file->pos >= device_size)
        return 0;
    if (size > device_size - file->pos)
        size = (uint32_t)(device_size - file->pos);

    uint8_t *secbuf = kmalloc(dev->sector_size);
    if (!secbuf)
        return -1;

    uint32_t done = 0;
    while (done < size) {
        uint64_t abs = (uint64_t)file->pos + done;
        uint64_t local_lba = abs / dev->sector_size;
        uint64_t lba = partition_start + local_lba;
        uint32_t off = abs % dev->sector_size;
        uint32_t chunk = dev->sector_size - off;

        if (chunk > (size - done))
            chunk = size - done;

        int read_result = is_partition ?
            block_read_partition(disk_id, partition_start, total_sectors,
                local_lba, 1, secbuf) :
            block_read_sector(disk_id, lba, secbuf, 1);
        if (read_result != 0) {
            kfree(secbuf);
            return done > 0 ? (int)done : -1;
        }

        memcpy(buf + done, secbuf + off, chunk);
        done += chunk;
    }

    kfree(secbuf);
    file->pos += done;
    return (int)done;
}

int devfs_write(vfs_file_t *file, const uint8_t *buf, uint32_t size) {
    if (!file || !buf)
        return -1;

    if (strcmp(file->rel_path, "null") == 0) {
        file->pos += size;
        return (int)size;
    }

    if (strcmp(file->rel_path, "rtc") == 0 ||
        strcmp(file->rel_path, "rtc0") == 0)
        return -1;

    if (strcmp(file->rel_path, "tty") == 0 ||
        strcmp(file->rel_path, "tty1") == 0) {
        for (uint32_t i = 0; i < size; ++i)
            putc((char)buf[i]);
        file->pos += size;
        return (int)size;
    }

    if (strcmp(file->rel_path, "zero") == 0)
        return -1;

    if (strcmp(file->rel_path, "random") == 0 || strcmp(file->rel_path, "urandom") == 0) {
        file->pos += size;
        return (int)size;
    }

    int disk_id = devfs_disk_id_from_name(file->rel_path);
    uint64_t partition_start = 0;
    uint64_t total_sectors = 0;
    bool is_partition = false;
    if (disk_id < 0) {
        for (int i = 0; i < general_partition_count; i++) {
            general_partition_t *part = &ahci_partitions[i];
            if (strcmp(part->name, file->rel_path) != 0)
                continue;
            disk_id = (int)part->ahci_port;
            partition_start = part->lba_start;
            total_sectors = part->sector_count;
            is_partition = true;
            break;
        }
    }
    if (disk_id < 0)
        return -1;

    block_device_info_t *dev = block_get_device(disk_id);
    if (!dev || dev->sector_size == 0)
        return -1;
    if (dev->type == BLOCK_DEVICE_USB && dev->total_sectors == 0 &&
        usb_msc_refresh_device(dev->backend_index) != 0)
        return -1;
    dev = block_get_device(disk_id);
    if (!dev)
        return -1;
    for (int i = 0; i < general_partition_count; i++) {
        if (strcmp(ahci_partitions[i].name, file->rel_path) == 0 &&
            ahci_partitions[i].fs_type == FS_ISO9660)
            return -1;
    }
    if (total_sectors == 0)
        total_sectors = dev->total_sectors;
    if (total_sectors > UINT64_MAX / dev->sector_size)
        return -1;
    uint64_t device_size = total_sectors * dev->sector_size;
    if (file->pos >= device_size)
        return 0;
    if (size > device_size - file->pos)
        size = (uint32_t)(device_size - file->pos);

    uint8_t *secbuf = kmalloc(dev->sector_size);
    if (!secbuf)
        return -1;

    uint32_t done = 0;
    while (done < size) {
        uint64_t abs = (uint64_t)file->pos + done;
        uint64_t local_lba = abs / dev->sector_size;
        uint64_t lba = partition_start + local_lba;
        uint32_t off = abs % dev->sector_size;
        uint32_t chunk = dev->sector_size - off;

        if (chunk > (size - done))
            chunk = size - done;

        if (off != 0 || chunk != dev->sector_size) {
            int read_result = is_partition ?
                block_read_partition(disk_id, partition_start, total_sectors,
                    local_lba, 1, secbuf) :
                block_read_sector(disk_id, lba, secbuf, 1);
            if (read_result != 0) {
                kfree(secbuf);
                return done > 0 ? (int)done : -1;
            }
        } else {
            memset(secbuf, 0, dev->sector_size);
        }

        memcpy(secbuf + off, buf + done, chunk);

        int write_result = is_partition ?
            block_write_partition(disk_id, partition_start, total_sectors,
                local_lba, 1, secbuf) :
            block_write_sector(disk_id, lba, secbuf, 1);
        if (write_result != 0) {
            kfree(secbuf);
            return done > 0 ? (int)done : -1;
        }

        done += chunk;
    }

    kfree(secbuf);
    file->pos += done;
    return (int)done;
}

void devfs_close(vfs_file_t *file) {
    (void)file;
}

int devfs_ls(void) {
    printfnoln(blue_color "null " reset_color);
    printfnoln(blue_color "zero " reset_color);
    printfnoln(blue_color "random " reset_color);
    printfnoln(blue_color "urandom " reset_color);
    printfnoln(blue_color "klog " reset_color);
    printfnoln(blue_color "syslog " reset_color);
    printfnoln(blue_color "tty " reset_color);
    printfnoln(blue_color "tty1 " reset_color);
    printfnoln(blue_color "rtc " reset_color);
    printfnoln(blue_color "rtc0 " reset_color);

    for (int i = 0; i < block_device_count; i++) {
        block_device_info_t *dev = &block_devices[i];
        if (!dev->present)
            continue;

        printfnoln(blue_color "%s " reset_color, dev->name);
    }

    for (int i = 0; i < general_partition_count; i++) {
        if (block_get_device((int)ahci_partitions[i].ahci_port))
            printfnoln(blue_color "%s " reset_color, ahci_partitions[i].name);
    }

    return 0;
}
