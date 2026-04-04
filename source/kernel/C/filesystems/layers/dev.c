/**
 * @file dev.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The dev folder to handle by the VFS.
 * @version 0.1
 * @date 2026-04-04
 */

#include <filesystems/layers/dev.h>
#include <ahci.h>
#include <basics.h>
#include <graphics.h>
#include <heap.h>
#include <strings.h>

static uint64_t devfs_rng_state = 0x9E3779B97F4A7C15ULL;

static int devfs_disk_id_from_name(const char* name)
{
    if (!name || !*name)
        return -1;

    for (int i = 0; i < block_device_count; i++) {
        block_device_info_t* dev = &block_devices[i];
        if (!dev->present)
            continue;

        if (dev->type != BLOCK_DEVICE_AHCI && dev->type != BLOCK_DEVICE_NVME)
            continue;

        if (strcmp(dev->name, name) == 0)
            return i;
    }

    return -1;
}

static bool devfs_is_block_node(const char* name)
{
    int id = devfs_disk_id_from_name(name);
    return id >= 0;
}

void devfs_init(void) {
    devfs_rng_state ^= rdtsc64();
}

static uint8_t devfs_next_rand_u8(void)
{
    devfs_rng_state ^= devfs_rng_state << 13;
    devfs_rng_state ^= devfs_rng_state >> 7;
    devfs_rng_state ^= devfs_rng_state << 17;
    return (uint8_t)(devfs_rng_state & 0xFF);
}

int devfs_open(vfs_file_t* file)
{
    if (!file || !file->rel_path)
        return -1;

    if (file->rel_path[0] == '\0') {
        file->pos = 0;
        return 0;
    }

    if (strcmp(file->rel_path, "null") == 0 ||
        strcmp(file->rel_path, "zero") == 0 ||
        strcmp(file->rel_path, "random") == 0 ||
        strcmp(file->rel_path, "urandom") == 0) {
        file->pos = 0;
        return 0;
    }

    if (devfs_is_block_node(file->rel_path)) {
        file->pos = 0;
        return 0;
    }

    return -1;
}

int devfs_read(vfs_file_t* file, uint8_t* buf, uint32_t size)
{
    if (!file || !buf)
        return -1;

    if (strcmp(file->rel_path, "null") == 0)
        return 0;

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

    int disk_id = devfs_disk_id_from_name(file->rel_path);
    if (disk_id < 0)
        return -1;

    block_device_info_t* dev = block_get_device(disk_id);
    if (!dev || dev->sector_size == 0)
        return -1;

    uint8_t* secbuf = kmalloc(dev->sector_size);
    if (!secbuf)
        return -1;

    uint32_t done = 0;
    while (done < size) {
        uint64_t abs = (uint64_t)file->pos + done;
        uint64_t lba = abs / dev->sector_size;
        uint32_t off = abs % dev->sector_size;
        uint32_t chunk = dev->sector_size - off;

        if (chunk > (size - done))
            chunk = size - done;

        if (block_read_sector(disk_id, lba, secbuf, 1) != 0) {
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

int devfs_write(vfs_file_t* file, const uint8_t* buf, uint32_t size)
{
    if (!file || !buf)
        return -1;

    if (strcmp(file->rel_path, "null") == 0) {
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
    if (disk_id < 0)
        return -1;

    block_device_info_t* dev = block_get_device(disk_id);
    if (!dev || dev->sector_size == 0)
        return -1;

    uint8_t* secbuf = kmalloc(dev->sector_size);
    if (!secbuf)
        return -1;

    uint32_t done = 0;
    while (done < size) {
        uint64_t abs = (uint64_t)file->pos + done;
        uint64_t lba = abs / dev->sector_size;
        uint32_t off = abs % dev->sector_size;
        uint32_t chunk = dev->sector_size - off;

        if (chunk > (size - done))
            chunk = size - done;

        if (off != 0 || chunk != dev->sector_size) {
            if (block_read_sector(disk_id, lba, secbuf, 1) != 0) {
                kfree(secbuf);
                return done > 0 ? (int)done : -1;
            }
        } else {
            memset(secbuf, 0, dev->sector_size);
        }

        memcpy(secbuf + off, buf + done, chunk);

        if (block_write_sector(disk_id, lba, secbuf, 1) != 0) {
            kfree(secbuf);
            return done > 0 ? (int)done : -1;
        }

        done += chunk;
    }

    kfree(secbuf);
    file->pos += done;
    return (int)done;
}

void devfs_close(vfs_file_t* file)
{
    (void)file;
}

int devfs_ls(void)
{
    printfnoln(blue_color "null " reset_color);
    printfnoln(blue_color "zero " reset_color);
    printfnoln(blue_color "random " reset_color);
    printfnoln(blue_color "urandom " reset_color);

    for (int i = 0; i < block_device_count; i++) {
        block_device_info_t* dev = &block_devices[i];
        if (!dev->present)
            continue;

        if (dev->type != BLOCK_DEVICE_AHCI && dev->type != BLOCK_DEVICE_NVME)
            continue;

        printfnoln(blue_color "%s " reset_color, dev->name);
    }

    return 0;
}
