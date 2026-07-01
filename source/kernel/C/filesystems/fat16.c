/**
 * @file fat16.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The main code for FAT16 Read & Write.
 * @version 0.1
 * @date 2025-12-28
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <filesystems/fat16.h>
#include <memory.h>
#include <debugger.h>
#include <strings.h>

typedef struct {
    char name[13];
    uint8_t attr;
} fat16_ls_entry_t;

static int fat16_ls_cmp(const void* a, const void* b)
{
    const fat16_ls_entry_t* ea = a;
    const fat16_ls_entry_t* eb = b;

    int da = (ea->attr & 0x10) != 0;
    int db = (eb->attr & 0x10) != 0;

    // Directories first
    if (da != db)
        return db - da;

    // Alphabetical
    return strcmp(ea->name, eb->name);
}

static int fat16_is_reserved_name(cstring name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

partition_fs_type_t detect_fat_type_enum(const uint8* buf) {
    fat16_boot_sector_t* bs = (fat16_boot_sector_t*)buf;
    const uint8_t* fat16_sig = (const uint8_t*)buf + 54;
    const uint8_t* fat32_sig = (const uint8_t*)buf + 82;

    if (memcmp(fat16_sig, "FAT12   ", 8) == 0)
        return FS_FAT12;

    if (memcmp(fat16_sig, "FAT16   ", 8) == 0)
        return FS_FAT16;

    if (memcmp(fat32_sig, "FAT32   ", 8) == 0)
        return FS_FAT32;

    if (bs->bytes_per_sector == 0 || bs->sectors_per_cluster == 0)
        return FS_UNKNOWN;

    if (bs->sectors_per_fat != 0 && bs->max_root_dir_entries != 0) {
        uint32_t total_sectors = (bs->total_sectors_short != 0) ? bs->total_sectors_short : bs->total_sectors_long;
        uint32_t data_sectors = total_sectors - (bs->reserved_sectors + bs->num_fats * bs->sectors_per_fat + ((bs->max_root_dir_entries * 32 + (bs->bytes_per_sector - 1)) / bs->bytes_per_sector));
        uint32_t cluster_count = data_sectors / bs->sectors_per_cluster;

        if (cluster_count < 4085) {
            return FS_FAT12;
        } else if (cluster_count < 65525) {
            return FS_FAT16;
        } else {
            return FS_UNKNOWN;
        }
    } else if (bs->max_root_dir_entries == 0) {
        return FS_FAT32;
    } else {
        return FS_UNKNOWN;
    }
}

int fat16_mount(int portno, uint32_t partition_lba, fat16_fs_t* fs) {
    uint8_t buf[512];

    if (ahci_read_sector(portno, partition_lba, buf, 1) != 0)
        return FAT_ERR_NOT_FOUND;

    fs->portno = portno;
    fs->partition_lba = partition_lba;
    memcpy(&fs->bs, buf, sizeof(fat16_boot_sector_t));

    fat16_boot_sector_t* bs = &fs->bs;

    fs->root_dir_sectors =
        ((bs->max_root_dir_entries * 32) + (bs->bytes_per_sector - 1)) /
        bs->bytes_per_sector;

    fs->fat_start = partition_lba + bs->reserved_sectors;
    fs->root_dir_start = fs->fat_start + (bs->num_fats * bs->sectors_per_fat);
    fs->data_start = fs->root_dir_start + fs->root_dir_sectors;
    fs->cwd_cluster = FAT16_ROOT_CLUSTER;
    strcpy(fs->cwd_path, "/");
    
    return FAT_OK;
}

void fat16_unmount(fat16_fs_t* fs)
{
    if (!fs)
        return;

    /*
     * FAT16 has no runtime state that must be flushed
     * because:
     *  - FAT updates are written immediately
     *  - directory entries are written immediately
     *  - no write-back cache exists
     */

    /* Clear sensitive fields (debug-friendly) */
    fs->portno = 0;
    fs->partition_lba = 0;
    fs->fat_start = 0;
    fs->root_dir_start = 0;
    fs->data_start = 0;
    fs->root_dir_sectors = 0;

    memset(&fs->bs, 0, sizeof(fs->bs));

    /* NOTE:
     * fs memory is freed by umount(), not here
     */
}

static int fat16_next_cluster_safe(fat16_fs_t* fs,
                                   uint16_t current,
                                   uint16_t* next,
                                   uint32_t* depth)
{
    if (current < 2 || current >= FAT16_EOC)
        return FAT_ERR_NOT_FOUND;

    if ((*depth)++ > FAT16_MAX_CLUSTERS) {
        printf("fat16: FAT loop detected\n");
        return FAT_ERR_CORRUPT;
    }

    *next = fat16_read_fat_fs(fs, current);
    return FAT_OK;
}

static int fat16_next_cluster_with_fallback(fat16_fs_t* fs,
                                            uint16_t current,
                                            uint16_t* next,
                                            uint32_t* depth)
{
    int rc = fat16_next_cluster_safe(fs, current, next, depth);
    if (rc != FAT_OK)
        return rc;

    /* A FAT entry of 0 in a live chain means corruption, not free cluster */
    if (*next == 0) {
        eprintf("fat16: FAT entry 0 in live chain at cluster %u", current);
        return FAT_ERR_CORRUPT;
    }

    return FAT_OK;
}


uint16_t fat16_read_fat_fs(fat16_fs_t* fs, uint16_t cluster) {
    uint8_t buf[512];
    uint32_t offset = (uint32_t)cluster * 2;
    uint32_t bps    = fs->bs.bytes_per_sector;
    uint32_t sector = fs->fat_start + offset / bps;
    uint32_t off    = offset % bps;

    ahci_read_sector(fs->portno, sector, buf, 1);

    /* If the 2-byte entry straddles a sector boundary, read the high byte
     * from the next sector. */
    if (off + 1 >= bps) {
        uint8_t buf2[512];
        ahci_read_sector(fs->portno, sector + 1, buf2, 1);
        return (uint16_t)buf[off] | ((uint16_t)buf2[0] << 8);
    }

    return *(uint16_t*)(buf + off);
}

// HELPERS ============
static uint32_t fat16_cluster_lba(fat16_fs_t* fs, uint16_t cluster) {
    if (cluster == 0) {
        return fs->root_dir_start;
    }
    return fs->data_start + (cluster - 2) * fs->bs.sectors_per_cluster;
}

static inline int fat16_dir_valid(fat16_dir_entry_t* e) {
    if (e->name[0] == 0x00) return 0; // end of directory
    if ((uint8_t)e->name[0] == 0xE5) return 0; // deleted
    if ((e->attr & 0x0F) == 0x0F) return 0;    // LFN entry
    if (e->attr & 0x08) return 0;              // volume label
    return 1;
}

int fat16_name_eq(const uint8_t fat_name[11], const char* input)
{
    char formatted[11];
    fat16_format_name(input, formatted);
    return memcmp((fat_name), (formatted), 11) == 0;
}

// END =========

int fat16_list_root(fat16_fs_t* fs)
{
    uint8_t buf[512];

    fat16_ls_entry_t entries[256];
    int count = 0;

    for (uint32_t s = 0; s < fs->root_dir_sectors; s++) {
        ahci_read_sector(fs->portno, fs->root_dir_start + s, buf, 1);

        fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

        for (int i = 0; i < 16; i++) {
            if (e[i].name[0] == 0x00)
                goto done;

            if ((uint8_t)e[i].name[0] == 0xE5) continue;
            if (e[i].attr & 0x08) continue;
            if ((e[i].attr & 0x0F) == 0x0F) continue;

            char name[13];
            fat16_unformat_name(&e[i], name);

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            entries[count].attr = e[i].attr;
            strcpy(entries[count].name, name);

            count++;
            if (count >= 256)
                goto done;
        }
    }

done:
    qsort(entries, count, sizeof(fat16_ls_entry_t), fat16_ls_cmp);

    for (int i = 0; i < count; i++) {
        if (entries[i].attr & 0x10)
            printfnoln(yellow_color "%s/ " reset_color, entries[i].name);
        else
            printfnoln(blue_color "%s " reset_color, entries[i].name);
    }

    return count;
}


int fat16_find_path(fat16_fs_t* fs, const char* path, fat16_dir_entry_t* out) {
    if (!fs || !path || !*path) return FAT_ERR_NOT_FOUND;

    uint16_t current_cluster = FAT16_ROOT_CLUSTER; // start at root
    const char* p = path;
    char part[13];

    // Skip leading slashes
    while (*p == '/') p++;

    // Root path
    if (*p == 0) {
        out->first_cluster = FAT16_ROOT_CLUSTER;
        out->attr = 0x10; // mark as directory
        return FAT_OK;
    }

    while (*p) {
        // Extract next path component
        int len = 0;
        while (p[len] && p[len] != '/') len++;
        if (len >= sizeof(part)) return FAT_ERR_NOT_FOUND;

        memcpy(part, p, len);
        part[len] = 0;

        fat16_dir_entry_t entry;
        int ret = fat16_find_in_dir(fs, current_cluster, part, &entry);
        if (ret != FAT_OK) return FAT_ERR_NOT_FOUND;

        // Update current cluster for next iteration
        current_cluster = entry.first_cluster;

        // If not last component, must be a directory
        if (p[len] == '/' && !(entry.attr & 0x10))
            return FAT_ERR_NOT_FOUND;

        p += len;
        while (*p == '/') p++;

        if (*p == 0) {
            // last component reached
            *out = entry;
            return FAT_OK;
        }
    }

    return FAT_ERR_NOT_FOUND;
}


int fat16_match_name(fat16_dir_entry_t* e, const char* name) {
    char fatname[11];
    fat16_format_name(name, fatname);
    return memcmp(e->name, fatname, 11) == 0;
}

int fat16_find_in_dir(
    fat16_fs_t* fs,
    uint16_t dir_cluster,
    const char* name,
    fat16_dir_entry_t* out)
{
    uint8_t buf[512];

    /* ---------- ROOT DIR ---------- */
    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < fs->root_dir_sectors; s++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00)
                    return FAT_ERR_NOT_FOUND;
                if ((uint8_t)e[i].name[0] == 0xE5)
                    continue;
                if ((e[i].attr & 0x0F) == 0x0F)
                    continue;
                if (e[i].attr & 0x08)
                    continue;

                if (fat16_name_eq(e[i].name, name)) {
                    *out = e[i];
                    return FAT_OK;
                }
            }
        }
        return FAT_ERR_NOT_FOUND;
    }

    /* ---------- SUBDIRECTORY ---------- */
    uint16_t cluster = dir_cluster;

    while (cluster >= 2 && cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00)
                    return FAT_ERR_NOT_FOUND;
                if ((uint8_t)e[i].name[0] == 0xE5)
                    continue;
                if ((e[i].attr & 0x0F) == 0x0F)
                    continue;
                if (e[i].attr & 0x08)
                    continue;


                if (fat16_name_eq(e[i].name, name)) {
                    debug_printf("FOUND '%s' cluster=%u size=%u\n",
                        name,
                        e[i].first_cluster,
                        e[i].filesize);
                    *out = e[i];
                    return FAT_OK;
                }
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return FAT_ERR_NOT_FOUND;
}

int fat16_list_dir_cluster(fat16_fs_t* fs, uint16_t start_cluster)
{
    uint8_t buf[512];
    uint16_t cluster = start_cluster;

    fat16_ls_entry_t entries[256];
    int count = 0;

    while (cluster >= 2 && cluster < 0xFFF8) {
        uint32_t lba =
            fs->data_start +
            (cluster - 2) * fs->bs.sectors_per_cluster;

        for (uint8_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);

            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < 16; i++) {
                if (e[i].name[0] == 0x00)
                    goto done;

                if ((uint8_t)e[i].name[0] == 0xE5) continue;
                if (e[i].attr & 0x08) continue;
                if ((e[i].attr & 0x0F) == 0x0F) continue;

                char name[13];
                fat16_unformat_name(&e[i], name);

                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                    continue;

                entries[count].attr = e[i].attr;
                strcpy(entries[count].name, name);

                count++;
                if (count >= 256)
                    goto done;
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

done:
    qsort(entries, count, sizeof(fat16_ls_entry_t), fat16_ls_cmp);

    for (int i = 0; i < count; i++) {
        if (entries[i].attr & 0x10)
            printfnoln(yellow_color "%s/ " reset_color, entries[i].name);
        else
            printfnoln(blue_color "%s " reset_color, entries[i].name);
    }

    return count;
}


void fat16_format_name(const char* input, char out[11]) {
    memset(out, ' ', 11);

    // special cases for "." and ".."
    if (strcmp(input, ".") == 0) {
        out[0] = '.';
        return;
    } else if (strcmp(input, "..") == 0) {
        out[0] = '.';
        out[1] = '.';
        return;
    }

    int i = 0, j = 0;

    // name
    while (input[i] && input[i] != '.' && j < 8)
        out[j++] = toupper((unsigned char) input[i++]);

    // extension
    if (input[i] == '.') {
        i++;
        j = 8;
        while (input[i] && j < 11)
            out[j++] = toupper((unsigned char) input[i++]);
    }
}


int fat16_find_file(fat16_fs_t* fs, const char* name, fat16_dir_entry_t* out) {
    uint8_t buf[512];
    char fatname[11];

    fat16_format_name(name, fatname);  // "HELLO.TXT" → "HELLO   TXT"

    for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
        ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
        fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

        for (int j = 0; j < 16; j++) {
            if (e[j].name[0] == 0x00) return FAT_ERR_NOT_FOUND;
            if ((uint8_t)e[j].name[0] == 0xE5) continue;
            if (e[j].attr & 0x08) continue; // volume label

            if (memcmp(e[j].name, fatname, 11) == 0) {
                *out = e[j];
                return FAT_OK;
            }
        }
    }
    return FAT_ERR_NOT_FOUND;
}

int fat16_open(fat16_fs_t* fs, const char* path, fat16_file_t* f) {
    if (!fs || !path || !f)
        return FAT_ERR_NOT_FOUND;

    memset(f, 0, sizeof(*f));

    while (*path == '/')
        path++;

    /* Allow opening the FAT16 root directory via empty relative path. */
    if (path[0] == '\0') {
        f->fs = fs;
        f->entry.attr = 0x10; /* directory */
        f->entry.first_cluster = FAT16_ROOT_CLUSTER;
        f->parent_cluster = FAT16_ROOT_CLUSTER;
        f->cluster = FAT16_ROOT_CLUSTER;
        f->pos = 0;
        return FAT_OK;
    }

    uint16_t parent;
    char name[13];

    if (fat16_find_parent(fs, path, &parent, name) != 0)
        return FAT_ERR_NOT_FOUND;

    fat16_dir_entry_t entry;
    if (fat16_find_in_dir(fs, parent, name, &entry) != 0)
        return FAT_ERR_NOT_FOUND;

    f->fs = fs;
    f->entry = entry;
    f->parent_cluster = parent;
    f->pos = 0;
    f->cluster = entry.first_cluster;

    return FAT_OK;
}

int fat16_read(fat16_file_t* f, uint8_t* out, uint32_t size) {
    if (!f || !f->fs || !out || size == 0)
        return 0;

    uint32_t read = 0;
    uint8_t sector[512];
    uint32_t depth = 0;
    uint32_t bps = f->fs->bs.bytes_per_sector;
    uint32_t cluster_size = f->fs->bs.sectors_per_cluster * bps;
    uint16_t cluster = f->entry.first_cluster;

    if (cluster_size == 0 || cluster < 2)
        return 0;

    uint32_t start_cluster_index = f->pos / cluster_size;
    for (uint32_t i = 0; i < start_cluster_index; ++i) {
        uint16_t next = 0;
        if (fat16_next_cluster_with_fallback(f->fs, cluster, &next, &depth) != FAT_OK) {
            eprintf("fat16: seek walk failed pos=%x idx=%u cluster=%u", f->pos, start_cluster_index, cluster);
            return 0;
        }
        cluster = next;
    }

    while (read < size && f->pos < f->entry.filesize) {
        uint32_t sector_in_cluster =
            (f->pos / bps) % f->fs->bs.sectors_per_cluster;

        uint32_t lba =
            fat16_cluster_lba(f->fs, cluster) + sector_in_cluster;

        ahci_read_sector(f->fs->portno, lba, sector, 1);

        uint32_t off = f->pos % bps;
        uint32_t to_copy = bps - off;

        if (to_copy > size - read)
            to_copy = size - read;
        if (to_copy > f->entry.filesize - f->pos)
            to_copy = f->entry.filesize - f->pos;

        memcpy(out + read, sector + off, to_copy);

        uint32_t cluster_index_before = f->pos / cluster_size;
        f->pos += to_copy;
        read += to_copy;
        f->cluster = cluster;

        if (read < size && f->pos < f->entry.filesize) {
            uint32_t cluster_index_after = f->pos / cluster_size;
            if (cluster_index_after != cluster_index_before) {
                uint16_t next = 0;
                if (fat16_next_cluster_with_fallback(f->fs, cluster, &next, &depth) != FAT_OK) {
                    eprintf("fat16: short read pos=%x read=%u want=%u cluster=%u", f->pos, read, size, cluster);
                    return read;
                }
                cluster = next;
            }
        }
    }

    return read;
}

int fat16_write(fat16_file_t* f, const uint8_t* data, uint32_t size)
{
    uint32_t written = 0;
    uint32_t bps = f->fs->bs.bytes_per_sector;
    uint8_t sector[512]; /* 512 is the max standard sector size; safe stack buffer */

    uint32_t cluster_size =
        f->fs->bs.sectors_per_cluster * bps;

    /* ---------- allocate first cluster ---------- */
    if (f->entry.first_cluster == 0) {
        uint16_t c = fat16_allocate_cluster(f->fs);
        if (!c)
            return written;

        fat16_write_fat_entry(f->fs, c, FAT16_EOC);
        f->entry.first_cluster = c;
        f->cluster = c;
    }

    /* Cache current cluster to avoid re-walking from head every sector */
    uint16_t cur_cluster = f->cluster;
    if (cur_cluster < 2)
        cur_cluster = f->entry.first_cluster;

    while (written < size) {
        uint32_t cluster_index = f->pos / cluster_size;
        uint32_t sector_in_cluster =
            (f->pos / bps) % f->fs->bs.sectors_per_cluster;

        /* Only re-walk from head if cluster cache is stale (e.g. seek) */
        uint16_t cluster = f->entry.first_cluster;
        for (uint32_t i = 0; i < cluster_index; i++) {
            uint16_t next = fat16_read_fat_fs(f->fs, cluster);

            if (next >= FAT16_EOC) {
                next = fat16_allocate_cluster(f->fs);
                if (!next)
                    return written;

                fat16_write_fat_entry(f->fs, cluster, next);
                fat16_write_fat_entry(f->fs, next, FAT16_EOC);
            }

            cluster = next;
        }

        cur_cluster = cluster;

        uint32_t lba =
            fat16_cluster_lba(f->fs, cluster) + sector_in_cluster;

        ahci_read_sector(f->fs->portno, lba, sector, 1);

        uint32_t off = f->pos % bps;
        uint32_t to_copy = bps - off;
        if (to_copy > size - written)
            to_copy = size - written;

        memcpy(sector + off, data + written, to_copy);
        ahci_write_sector(f->fs->portno, lba, sector, 1);

        f->pos += to_copy;
        written += to_copy;
    }

    f->cluster = cur_cluster;

    /* Only update filesize if we extended the file */
    if (f->pos > f->entry.filesize)
        f->entry.filesize = f->pos;

    /* ---------- update directory entry ---------- */
    if (f->parent_cluster == 0)
        fat16_update_root_entry(f->fs, &f->entry);
    else
        fat16_update_dir_entry(f->fs, f->parent_cluster, &f->entry);

    return written;
}

void fat16_close(fat16_file_t* f) {
    memset(f, 0, sizeof(*f));
}

// WRITE WITH EXTEND = START ========
uint16_t fat16_find_free_cluster(fat16_fs_t* fs) {
    uint8_t sector[512];
    uint32_t bps = fs->bs.bytes_per_sector;
    uint32_t entries_per_sector = bps / 2; /* each FAT16 entry is 2 bytes */

    for (uint32_t s = 0; s < fs->bs.sectors_per_fat; s++) {
        ahci_read_sector(fs->portno, fs->fat_start + s, sector, 1);

        uint16_t* fat = (uint16_t*)sector;
        for (uint32_t i = 0; i < entries_per_sector; i++) {
            uint32_t cluster = s * entries_per_sector + i;

            if (cluster < 2)
                continue;

            if (cluster >= 0xFFF8) /* past valid cluster range */
                return 0;

            if (fat[i] == 0x0000)
                return (uint16_t)cluster;
        }
    }

    return 0; /* disk full */
}

void fat16_write_fat_entry(fat16_fs_t* fs, uint16_t cluster, uint16_t value) {
    uint32_t offset  = (uint32_t)cluster * 2;
    uint32_t bps     = fs->bs.bytes_per_sector;
    uint32_t sec_off = offset / bps;
    uint32_t byte_off = offset % bps;

    uint8_t buf[512];

    /* Write to every FAT copy */
    for (uint8_t fat_n = 0; fat_n < fs->bs.num_fats; fat_n++) {
        uint32_t fat_base = fs->fat_start + fat_n * fs->bs.sectors_per_fat;
        uint32_t sector   = fat_base + sec_off;

        ahci_read_sector(fs->portno, sector, buf, 1);

        if (byte_off + 1 >= bps) {
            /* Entry straddles sector boundary */
            buf[byte_off] = (uint8_t)(value & 0xFF);
            ahci_write_sector(fs->portno, sector, buf, 1);

            ahci_read_sector(fs->portno, sector + 1, buf, 1);
            buf[0] = (uint8_t)(value >> 8);
            ahci_write_sector(fs->portno, sector + 1, buf, 1);
        } else {
            *(uint16_t*)(buf + byte_off) = value;
            ahci_write_sector(fs->portno, sector, buf, 1);
        }
    }
}

uint16_t fat16_allocate_cluster(fat16_fs_t* fs) {
    uint16_t cluster = fat16_find_free_cluster(fs);
    if (cluster == 0)
        return 0;

    fat16_write_fat_entry(fs, cluster, FAT16_EOC);

    /* Zero out the newly allocated cluster */
    uint8_t zero[512];
    memset(zero, 0, sizeof(zero));
    uint32_t lba = fat16_cluster_lba(fs, cluster);

    for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++)
        ahci_write_sector(fs->portno, lba + s, zero, 1);

    return cluster;
}

uint16_t fat16_append_cluster(fat16_fs_t* fs, uint16_t last_cluster) {
    uint16_t new_cluster = fat16_allocate_cluster(fs);
    if (!new_cluster)
        return 0;

    fat16_write_fat_entry(fs, last_cluster, new_cluster);
    fat16_write_fat_entry(fs, new_cluster, FAT16_EOC);

    return new_cluster;
}

void fat16_update_root_entry(
    fat16_fs_t* fs,
    fat16_dir_entry_t* entry
) {
    uint8_t buf[512];

    for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
        ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
        fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

        for (int j = 0; j < 16; j++) {
            if (e[j].name[0] == 0x00)
                return;

            if ((uint8_t)e[j].name[0] == 0xE5)
                continue;

            if (e[j].attr == 0x0F)
                continue;

            if (memcmp(e[j].name, entry->name, 11) == 0) {
                e[j] = *entry;
                ahci_write_sector(
                    fs->portno,
                    fs->root_dir_start + i,
                    buf,
                    1
                );
                return;
            }
        }
    }
}

void fat16_free_chain(fat16_fs_t* fs, uint16_t start_cluster)
{
    uint16_t cluster = start_cluster;
    uint32_t depth = 0;

    while (cluster >= 2 && cluster < FAT16_EOC) {
        uint16_t next;

        if (fat16_next_cluster_safe(fs, cluster, &next, &depth) != FAT_OK)
            break;

        fat16_write_fat_entry(fs, cluster, 0x0000);
        cluster = next;
    }
}



int fat16_update_dir_entry(
    fat16_fs_t* fs,
    uint16_t dir_cluster,
    fat16_dir_entry_t* entry
) {
    uint8_t buf[512];
    uint16_t cluster = dir_cluster;

    while (cluster >= 2 && cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00)
                    return FAT_ERR_NOT_FOUND; /* end of directory */
                if ((uint8_t)e[i].name[0] == 0xE5)
                    continue; /* deleted entry */
                if ((e[i].attr & 0x0F) == 0x0F)
                    continue; /* LFN */

                if (memcmp(e[i].name, entry->name, 11) == 0) {
                    e[i] = *entry;
                    ahci_write_sector(fs->portno, lba + s, buf, 1);
                    return FAT_OK;
                }
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return FAT_ERR_NOT_FOUND;
}

int fat16_find_free_dir_entry(
    fat16_fs_t* fs,
    uint16_t dir_cluster,
    uint32_t* out_lba,
    uint32_t* out_index
) {
    uint8_t buf[512];

    /* ---------- ROOT DIR (fixed-size, not cluster-based) ---------- */
    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < fs->root_dir_sectors; s++) {
            uint32_t lba = fs->root_dir_start + s;
            ahci_read_sector(fs->portno, lba, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00 || (uint8_t)e[i].name[0] == 0xE5) {
                    *out_lba   = lba;
                    *out_index = (uint32_t)i;
                    return FAT_OK;
                }
            }
        }
        return FAT_ERR_NOT_FOUND; /* root dir is full - cannot extend */
    }

    /* ---------- SUBDIRECTORY ---------- */
    uint16_t cluster = dir_cluster;
    uint32_t depth   = 0;

    while (cluster >= 2 && cluster < FAT16_EOC) {
        if (depth++ > FAT16_MAX_CLUSTERS)
            return FAT_ERR_CORRUPT;

        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00 || (uint8_t)e[i].name[0] == 0xE5) {
                    *out_lba   = lba + s;
                    *out_index = (uint32_t)i;
                    return FAT_OK;
                }
            }
        }

        uint16_t next = fat16_read_fat_fs(fs, cluster);
        if (next >= FAT16_EOC) {
            next = fat16_append_cluster(fs, cluster);
            if (!next) return FAT_ERR_NOT_FOUND;
        }
        cluster = next;
    }

    return FAT_ERR_NOT_FOUND;
}

int fat16_create(fat16_fs_t* fs, uint16_t parent_cluster, const char* name, uint8_t attr) {
    if (fat16_is_reserved_name(name)) {
        printf("refusing to create reserved name '%s'", name);
        return FAT_ERR_NOT_FOUND;
    }

    // If directory, delegate to mkdir
    if (attr & 0x10) {
        return fat16_mkdir(fs, parent_cluster, name);
    }

    // Check if file already exists
    fat16_dir_entry_t tmp;
    if (fat16_find_in_dir(fs, parent_cluster, name, &tmp) == 0) {
        printf("create: '%s' already exists", name);
        return FAT_ERR_NOT_FOUND;
    }

    fat16_dir_entry_t e;
    memset(&e, 0, sizeof(e));
    fat16_format_name(name, e.name);
    e.attr = attr;
    e.first_cluster = 0;
    e.filesize = 0;

    uint8_t buf[512];

    if (parent_cluster == 0) { // root directory
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* d = (fat16_dir_entry_t*)buf;
            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (d[j].name[0] == 0x00 || (uint8_t)d[j].name[0] == 0xE5) {
                    d[j] = e;
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, buf, 1);
                    return FAT_OK;
                }
            }
        }
        return FAT_ERR_NOT_FOUND;
    }

    // subdirectory
    uint32_t lba, idx;
    if (fat16_find_free_dir_entry(fs, parent_cluster, &lba, &idx) != 0)
        return FAT_ERR_NOT_FOUND;

    ahci_read_sector(fs->portno, lba, buf, 1);
    ((fat16_dir_entry_t*)buf)[idx] = e;
    ahci_write_sector(fs->portno, lba, buf, 1);
    return FAT_OK;
}

int fat16_delete_entry_in_cluster(fat16_fs_t* fs, uint16_t cluster, const char* fatname) {
    uint8_t buf[512];

    while (cluster >= 2 && cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* d = (fat16_dir_entry_t*)buf;

            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(d[j].name, fatname, 11) == 0) {
                    memset(d[j].name, 0xE5, 11); // mark deleted
                    ahci_write_sector(fs->portno, lba + s, buf, 1);
                    return FAT_OK;
                }
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return FAT_ERR_NOT_FOUND;
}

int fat16_unlink(fat16_fs_t* fs, uint16_t parent_cluster, const char* name) {
    if (!fs || !name) return FAT_ERR_NOT_FOUND;

    fat16_dir_entry_t e;
    char fatname[11];
    fat16_format_name(name, fatname);

    if (fat16_is_reserved_name(name))
        return FAT_ERR_NOT_FOUND;

    if (fat16_find_in_dir(fs, parent_cluster, name, &e) != 0) {
        printf("unlink: '%s' not found", name);
        return FAT_ERR_NOT_FOUND;
    }

    if (e.attr & 0x10) {
        printf("unlink: '%s' is a directory", name);
        return FAT_ERR_NOT_FOUND;
    }

    // Free cluster chain
    if (e.first_cluster >= 2) {
        fat16_free_chain(fs, e.first_cluster);
    }

    if (parent_cluster == 0) {
        // root directory
        uint8_t buf[512];
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* d = (fat16_dir_entry_t*)buf;

            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(d[j].name, fatname, 11) == 0) {
                    memset(d[j].name, 0xE5, 11);
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, buf, 1);
                    return FAT_OK;
                }
            }
        }
        return FAT_ERR_NOT_FOUND;
    }

    // Subdirectory
    return fat16_delete_entry_in_cluster(fs, parent_cluster, fatname);
}

int fat16_truncate(fat16_file_t* f, uint32_t new_size)
{
    uint32_t cluster_size =
        f->fs->bs.sectors_per_cluster * f->fs->bs.bytes_per_sector;

    /* ---------- truncate to zero ---------- */
    if (new_size == 0) {
        if (f->entry.first_cluster >= 2)
            fat16_free_chain(f->fs, f->entry.first_cluster);

        f->entry.first_cluster = 0;
        f->entry.filesize = 0;
        f->pos = 0;
        f->cluster = 0;
        return FAT_OK;
    }

    /* ---------- no-op grow ---------- */
    if (new_size >= f->entry.filesize)
        return FAT_OK;

    uint32_t keep_clusters =
        (new_size + cluster_size - 1) / cluster_size;

    uint16_t cluster = f->entry.first_cluster;
    uint16_t prev    = 0;

    for (uint32_t i = 0; i < keep_clusters; i++) {
        if (cluster < 2 || cluster >= FAT16_EOC)
            break; /* chain shorter than expected - stop */
        prev    = cluster;
        cluster = fat16_read_fat_fs(f->fs, cluster);
    }

    if (prev != 0) {
        /* Terminate the chain at the last kept cluster */
        fat16_write_fat_entry(f->fs, prev, FAT16_EOC);
    } else {
        /* keep_clusters == 0: free entire chain and zero first_cluster */
        cluster = f->entry.first_cluster;
        f->entry.first_cluster = 0;
    }

    /* Free everything from cluster onwards */
    if (cluster >= 2 && cluster < FAT16_EOC)
        fat16_free_chain(f->fs, cluster);

    f->entry.filesize = new_size;
    if (f->pos > new_size)
        f->pos = new_size;

    return FAT_OK;
}


int fat16_mkdir(fat16_fs_t* fs, uint16_t parent_cluster, const char* name) {
    if (fat16_is_reserved_name(name)) {
        printf("refusing to create reserved name '%s'", name);
        return FAT_ERR_NOT_FOUND;
    }
    
    fat16_dir_entry_t tmp;
    if (fat16_find_in_dir(fs, parent_cluster, name, &tmp) == 0) {
        printf("mkdir: directory '%s' already exists", name);
        return FAT_ERR_NOT_FOUND;
    }

    uint16_t new_cluster = fat16_allocate_cluster(fs);
    if (!new_cluster) return FAT_ERR_NOT_FOUND;

    /* ---------- initialize directory cluster ---------- */
    uint32_t lba = fat16_cluster_lba(fs, new_cluster);
    uint8_t sector[512];
    memset(sector, 0, sizeof(sector));

    fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector;

    // "."
    fat16_format_name(".", entries[0].name);
    entries[0].attr = 0x10;
    entries[0].first_cluster = new_cluster;

    // ".."
    fat16_format_name("..", entries[1].name);
    entries[1].attr = 0x10;
    entries[1].first_cluster = (parent_cluster == 0) ? 0 : parent_cluster;

    ahci_write_sector(fs->portno, lba, sector, 1);

    /* ---------- create directory entry in parent ---------- */
    fat16_dir_entry_t dir;
    memset(&dir, 0, sizeof(dir));
    fat16_format_name(name, dir.name);
    dir.attr = 0x10;
    dir.first_cluster = new_cluster;
    dir.filesize = 0;

    uint32_t out_lba, out_idx;
    if (parent_cluster == 0) {
        // root dir
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, sector, 1);
            fat16_dir_entry_t* d = (fat16_dir_entry_t*)sector;

            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (d[j].name[0] == 0x00 || (uint8_t)d[j].name[0] == 0xE5) {
                    d[j] = dir;
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, sector, 1);
                    return FAT_OK;
                }
            }
        }
        return FAT_ERR_NOT_FOUND;
    }

    if (fat16_find_free_dir_entry(fs, parent_cluster, &out_lba, &out_idx) != 0)
        return FAT_ERR_NOT_FOUND;

    ahci_read_sector(fs->portno, out_lba, sector, 1);
    ((fat16_dir_entry_t*)sector)[out_idx] = dir;
    ahci_write_sector(fs->portno, out_lba, sector, 1);

    return FAT_OK;
}

static int fat16_dir_is_empty(fat16_fs_t* fs, uint16_t cluster)
{
    uint8_t buf[512];

    while (cluster >= 2 && cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00)
                    return FAT_OK; // end → empty

                if ((uint8_t)e[i].name[0] == 0xE5)
                    continue;

                if (fat16_name_eq(e[i].name, ".") ||
                    fat16_name_eq(e[i].name, ".."))
                    continue;

                return FAT_ERR_NOT_EMPTY;
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return FAT_OK;
}

int fat16_create_path(fat16_fs_t* fs,
                      const char* path,
                      uint16_t start_cluster,
                      uint8_t attr)
{
    if (!fs || !path || !*path)
        return FAT_ERR_NOT_FOUND;

    char part[13];
    uint16_t current_cluster = start_cluster;
    const char* p = path;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        int len = 0;
        while (p[len] && p[len] != '/') len++;

        if (len >= sizeof(part))
            return FAT_ERR_NOT_FOUND;  // name too long for FAT16

        memcpy(part, p, len);
        part[len] = 0;

        /* BLOCK "." and ".." */
        if (fat16_is_reserved_name(part)) {
            printf("create: refusing to create reserved name \'%s\'", part);
            return FAT_ERR_NOT_FOUND;
        }

        /* Detect last component properly */
        const char* next = p + len;
        while (*next == '/') next++;
        int is_last = (*next == '\0');

        uint8_t final_attr = is_last ? attr : 0x10;

        fat16_dir_entry_t entry;
        int exists = (fat16_find_in_dir(fs, current_cluster, part, &entry) == 0);

        if (!exists) {
            if (fat16_create(fs, current_cluster, part, final_attr) != 0)
                return FAT_ERR_NOT_FOUND;
        } else {
            /* If last component already exists → error */
            if (is_last) {
                printf("create: '%s' already exists", part);
                return FAT_ERR_NOT_FOUND;
            }

            /* Must be directory if not last */
            if (!(entry.attr & 0x10)) {
                printf("create: '%s' is not a directory", part);
                return FAT_ERR_NOT_FOUND;
            }
        }

        if (!is_last) {
            if (fat16_find_in_dir(fs, current_cluster, part, &entry) != 0)
                return FAT_ERR_NOT_FOUND;
            current_cluster = entry.first_cluster;
        }

        p += len;
    }

    return FAT_OK;
}

int fat16_find_parent(
    fat16_fs_t* fs,
    const char* path,
    uint16_t* out_cluster,
    char* out_name
) {
    if (!fs || !path || !out_cluster || !out_name)
        return FAT_ERR_NOT_FOUND;

    const char* last_slash = strrchr(path, '/');

    /* ---------- NO SLASH → current directory ---------- */
    if (!last_slash) {
        if (strlen(path) >= 13)
            return FAT_ERR_NOT_FOUND; /* name too long for FAT16 8.3 */
        *out_cluster = FAT16_ROOT_CLUSTER;
        strcpy(out_name, path);
        return FAT_OK;
    }

    /* ---------- ROOT (/file) ---------- */
    if (last_slash == path) {
        if (strlen(last_slash + 1) >= 13)
            return FAT_ERR_NOT_FOUND;
        *out_cluster = 0;
        strcpy(out_name, last_slash + 1);
        return FAT_OK;
    }

    /* ---------- SUBDIRECTORY ---------- */
    char parent_path[128];
    size_t len = (size_t)(last_slash - path);
    if (len >= sizeof(parent_path))
        return FAT_ERR_NOT_FOUND; /* path too long */
    if (strlen(last_slash + 1) >= 13)
        return FAT_ERR_NOT_FOUND; /* final component too long for FAT16 8.3 */

    memcpy(parent_path, path, len);
    parent_path[len] = 0;

    fat16_dir_entry_t entry;
    if (fat16_find_path(fs, parent_path, &entry) != 0)
        return FAT_ERR_NOT_FOUND;

    if (!(entry.attr & 0x10))
        return FAT_ERR_NOT_FOUND; /* parent component is not a directory */

    *out_cluster = entry.first_cluster;
    strcpy(out_name, last_slash + 1);
    return FAT_OK;
}

int fat16_delete_entry(fat16_fs_t* fs, uint16_t parent_cluster, const char* name) {
    fat16_dir_entry_t e;
    if (fat16_find_in_dir(fs, parent_cluster, name, &e) != 0) return FAT_ERR_NOT_FOUND;

    if (e.first_cluster >= 2)
        fat16_free_chain(fs, e.first_cluster);

    uint8_t buf[512];
    char fatname[11];
    fat16_format_name(name, fatname);

    if (fat16_is_reserved_name(name))
        return FAT_ERR_NOT_FOUND;

    // Root directory
    if (parent_cluster == 0) {
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(entries[j].name, fatname, 11) == 0) {
                    entries[j].name[0] = 0xE5;
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, buf, 1);
                    return FAT_OK;
                }
            }
        }
        return FAT_ERR_NOT_FOUND;
    }

    // Subdirectory
    uint16_t cluster = parent_cluster;
    while (cluster >= 2 && cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);
        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(entries[j].name, fatname, 11) == 0) {
                    entries[j].name[0] = 0xE5;
                    ahci_write_sector(fs->portno, lba + s, buf, 1);
                    return FAT_OK;
                }
            }
        }
        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return FAT_ERR_NOT_FOUND;
}

void fat16_unformat_name(const fat16_dir_entry_t* e, char* out) {
    int i = 0;
    // Copy base name (bytes 0-7 of the 11-byte 8.3 field)
    for (int j = 0; j < 8 && e->name[j] != ' '; j++)
        out[i++] = e->name[j];

    // Copy extension if present (bytes 8-10 of the same 11-byte field)
    if (e->name[8] != ' ') {
        out[i++] = '.';
        for (int j = 8; j < 11 && e->name[j] != ' '; j++)
            out[i++] = e->name[j];
    }
    /* i is bounded to at most 8 + 1 + 3 = 12, so out[12] (the NUL) is
     * always within a 13-byte buffer as used by every call site. */
    out[i] = 0;
}

int fat16_ls(fat16_fs_t* fs, const char* path) {
    fat16_dir_entry_t dir;

    // Resolve directory
    if (strcmp(path, "/") == 0) {
        dir.first_cluster = 0;
        dir.attr = 0x10;
    } else {
        if (fat16_find_path(fs, path, &dir) != 0) {
            printf("ls: cannot access '%s'", path);
            return FAT_ERR_NOT_FOUND;
        }

        if (!(dir.attr & 0x10)) {
            printf("ls: '%s' is not a directory", path);
            return FAT_ERR_NOT_FOUND;
        }
    }
    
    if (dir.first_cluster == 0) {
        fat16_list_root(fs);
    } else {
        fat16_list_dir_cluster(fs, dir.first_cluster);
    }

    return FAT_OK;
}

int fat16_cd(fat16_fs_t* fs, const char* path, uint16_t* pwd_cluster) {
    uint16_t new_cluster = 0;
    uint16_t current = *pwd_cluster;

    if (fat16_resolve_path(fs, path, current, &new_cluster) != 0) {
        printf("cd: no such directory: %s", path);
        return FAT_ERR_NOT_FOUND;
    }

    *pwd_cluster = new_cluster;
    return FAT_OK;
}

int fat16_resolve_path(
    fat16_fs_t* fs,
    const char* path,
    uint16_t pwd_cluster,
    uint16_t* out_cluster
) {
    char part[13];
    uint16_t current = (path[0] == '/') ? 0 : pwd_cluster;
    const char* p = path;

    while (*p == '/') p++;

    while (*p) {
        int len = 0;
        while (p[len] && p[len] != '/') len++;

        if (len >= (int)sizeof(part))
            return FAT_ERR_NOT_FOUND; /* component too long for FAT16 8.3 */

        memcpy(part, p, len);
        part[len] = 0;

        if (strcmp(part, ".") == 0) {
            /* no-op */
        }
        else if (strcmp(part, "..") == 0) {
            if (current == 0) {
                /* stay at root */
            } else {
                fat16_dir_entry_t e;
                if (fat16_find_in_dir(fs, current, "..", &e) == 0)
                    current = e.first_cluster;
                else
                    current = 0;
            }
        }
        else {
            fat16_dir_entry_t e;
            if (fat16_find_in_dir(fs, current, part, &e) != 0)
                return FAT_ERR_NOT_FOUND;
            if (!(e.attr & 0x10))
                return FAT_ERR_NOT_FOUND;
            current = e.first_cluster;
        }

        p += len;
        while (*p == '/') p++;
    }

    *out_cluster = current;
    return FAT_OK;
}

int fat16_ls_cwd(fat16_fs_t* fs, const char* path) {
    if (!path || path[0] == '\0') {
        path = fs->cwd_path;
    }
    return fat16_ls(fs, path);
}

int fat16_unlink_path(fat16_fs_t* fs, uint16_t parent_cluster, const char* name) {
    fat16_dir_entry_t e;
    char fatname[11];
    fat16_format_name(name, fatname);

    if (fat16_find_in_dir(fs, parent_cluster, name, &e) != 0)
        return FAT_ERR_NOT_FOUND; // not found

    // Directory?
    if (e.attr & 0x10) {
        // Check empty
        uint8_t buf[512];
        uint16_t cluster = e.first_cluster;
        int empty = 1;

        while (cluster >= 2 && cluster < FAT16_EOC) {
            uint32_t lba = fat16_cluster_lba(fs, cluster);
            for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
                ahci_read_sector(fs->portno, lba + s, buf, 1);
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
                for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                    if (entries[i].name[0] != 0x00 && (uint8_t)entries[i].name[0] != 0xE5) {
                        // skip "." and ".."
                        if (!(entries[i].name[0] == '.' && (entries[i].name[1] == ' ' || entries[i].name[1] == '.')))
                            empty = 0;
                    }
                }
            }
            cluster = fat16_read_fat_fs(fs, cluster);
        }

        if (!empty) return FAT_ERR_NOT_EMPTY;

        // Free cluster chain
        if (e.first_cluster >= 2)
            fat16_free_chain(fs, e.first_cluster);
    } else {
        // Free file clusters
        if (e.first_cluster >= 2)
            fat16_free_chain(fs, e.first_cluster);
    }

    // Delete entry from parent
    if (parent_cluster == 0) {
        uint8_t buf[512];
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(entries[j].name, fatname, 11) == 0) {
                    memset(entries[j].name, 0xE5, 11);
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, buf, 1);
                    return FAT_OK;
                }
            }
        }
    } else {
        return fat16_delete_entry_in_cluster(fs, parent_cluster, fatname);
    }

    return FAT_ERR_NOT_FOUND;
}

int fat16_rmdir(fat16_fs_t* fs, uint16_t dir_cluster)
{
    uint8_t buf[512];

    if (dir_cluster < 2)
        return FAT_ERR_NOT_FOUND;

    uint16_t cluster = dir_cluster;

    /* ---------- PASS 1: recurse & free children ---------- */
    while (cluster >= 2 && cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00)
                    goto pass2;

                if ((uint8_t)e[i].name[0] == 0xE5)
                    continue;

                if (fat16_name_eq(e[i].name, ".") ||
                    fat16_name_eq(e[i].name, ".."))
                    continue;

                if (e[i].first_cluster < 2)
                    continue;

                if (e[i].attr & 0x10)
                    fat16_rmdir(fs, e[i].first_cluster);
                else
                    fat16_free_chain(fs, e[i].first_cluster);

                e[i].name[0] = (char)0xE5;
                ahci_write_sector(fs->portno, lba + s, buf, 1);
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

pass2:
    cluster = dir_cluster;

    /* ---------- PASS 2: mark entries deleted ---------- */
    while (cluster >= 2 && cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00)
                    goto done;

                if (fat16_name_eq(e[i].name, ".") ||
                    fat16_name_eq(e[i].name, ".."))
                    continue;

                e[i].name[0] = (char)0xE5;
            }

            ahci_write_sector(fs->portno, lba + s, buf, 1);
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

done:
    fat16_free_chain(fs, dir_cluster);
    return FAT_OK;
}

int fat16_mv(
    fat16_fs_t* fs,
    uint16_t src_parent,
    const char* src_name,
    uint16_t dst_parent,
    const char* dst_name
) {
    fat16_dir_entry_t src_entry;

    // 1. Find source
    if (fat16_find_in_dir(fs, src_parent, src_name, &src_entry) != FAT_OK)
        return FAT_ERR_NOT_FOUND;

    // 2. Ensure destination does NOT exist
    fat16_dir_entry_t tmp;
    if (fat16_find_in_dir(fs, dst_parent, dst_name, &tmp) == FAT_OK)
        return FAT_ERR_EXISTS;

    // 3. Prepare new entry
    fat16_dir_entry_t new_entry = src_entry;
    fat16_format_name(dst_name, new_entry.name);

    // 4. Insert into destination directory
    uint32_t lba, idx;
    if (dst_parent == 0) {
        uint8_t buf[512];
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* d = (fat16_dir_entry_t*)buf;
            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (d[j].name[0] == 0x00 || (uint8_t)d[j].name[0] == 0xE5) {
                    d[j] = new_entry;
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, buf, 1);
                    goto inserted;
                }
            }
        }
        return FAT_ERR_NOT_FOUND;
    } else {
        if (fat16_find_free_dir_entry(fs, dst_parent, &lba, &idx) != FAT_OK)
            return FAT_ERR_NOT_FOUND;

        uint8_t buf[512];
        ahci_read_sector(fs->portno, lba, buf, 1);
        ((fat16_dir_entry_t*)buf)[idx] = new_entry;
        ahci_write_sector(fs->portno, lba, buf, 1);
    }

inserted:
    // 5. Delete source entry (DO NOT FREE CLUSTERS)
    char fatname[11];
    fat16_format_name(src_name, fatname);

    if (src_parent == 0) {
        uint8_t buf[512];
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* d = (fat16_dir_entry_t*)buf;
            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(d[j].name, fatname, 11) == 0) {
                    d[j].name[0] = 0xE5;
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, buf, 1);
                    return FAT_OK;
                }
            }
        }
    } else {
        return fat16_delete_entry_in_cluster(fs, src_parent, fatname);
    }

    return FAT_OK;
}

int fat16_sync(fat16_fs_t *fs)
{
    if (!fs)
        return -1;

    /*
     * Future work:
     *  - Flush FAT cache
     *  - Flush directory cache
     *  - Flush data block cache
     *  - Issue ATA FLUSH CACHE if supported
     */

    return 0;
}