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
#include <strings.h>
#include <ahci.h>

int detect_fat_type(int8* buf) {
    fat16_boot_sector_t* bs = (fat16_boot_sector_t*)buf;

    if (bs->sectors_per_fat != 0 && bs->max_root_dir_entries != 0) {
        uint32_t total_sectors = (bs->total_sectors_short != 0) ? bs->total_sectors_short : bs->total_sectors_long;
        uint32_t data_sectors = total_sectors - (bs->reserved_sectors + bs->num_fats * bs->sectors_per_fat + ((bs->max_root_dir_entries * 32 + (bs->bytes_per_sector - 1)) / bs->bytes_per_sector));
        uint32_t cluster_count = data_sectors / bs->sectors_per_cluster;

        if (cluster_count < 4085) {
            return 12;
        } else if (cluster_count < 65525) {
            return 16;
        } else {
            return 1;
        }
    } else if (bs->max_root_dir_entries == 0) {
        return 32;
    } else {
        return 0;
    }
}

partition_fs_type_t detect_fat_type_enum(const int8* buf){
    int fat = detect_fat_type(buf);

    switch(fat){
        case 12:
            return FS_FAT12;
        case 16:
            return FS_FAT16;
        case 32:
            return FS_FAT32;
        case 1:
        case 0:
        default:
            return FS_UNKNOWN;
    }
}

int fat16_mount(int portno, uint32_t partition_lba, fat16_fs_t* fs) {
    uint8_t buf[512];

    if (ahci_read_sector(portno, partition_lba, buf, 1) != 0)
        return -1;

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

    printf("mount: successfully mounted FAT16 partition");
    return 0;
}

uint16_t fat16_read_fat_fs(fat16_fs_t* fs, uint16_t cluster) {
    uint8_t buf[512];
    uint32_t offset = cluster * 2;
    uint32_t sector = fs->fat_start + offset / fs->bs.bytes_per_sector;

    ahci_read_sector(fs->portno, sector, buf, 1);
    return *(uint16_t*)(buf + (offset % fs->bs.bytes_per_sector));
}

// HELPERS ============
static inline uint32_t fat16_cluster_lba(fat16_fs_t* fs, uint16_t cluster) {
    return fs->data_start + (cluster - 2) * fs->bs.sectors_per_cluster;
}

static inline int fat16_dir_valid(fat16_dir_entry_t* e) {
    if (e->name[0] == 0x00) return 0; // end
    if (e->name[0] == 0xE5) return -1; // deleted
    if (e->attr == 0x0F) return -1;    // LFN
    if (e->attr & 0x08) return -1;     // volume label
    return 1;
}
// END =========

void fat16_list_root(fat16_fs_t* fs) {
    uint8_t buf[512];

    for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
        ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
        fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

        for (int idx = 0; idx < 16; idx++) {
            if (e[idx].name[0] == 0x00) return;   // end
            if (e[idx].name[0] == 0xE5) continue; // deleted
            if (e[idx].attr & 0x08) continue;     // volume label

            char name[9], ext[4];
            memcpy(name, e[idx].name, 8);
            memcpy(ext,  e[idx].ext,  3);
            name[8] = 0;
            ext[3]  = 0;

            // trim spaces
            for (int k = 7; k >= 0 && name[k] == ' '; k--) name[k] = 0;
            for (int k = 2; k >= 0 && ext[k]  == ' '; k--) ext[k]  = 0;

            if (e[idx].attr & 0x10) {
                // directory
                printf("[DIR ] %s (cluster=%u)", name, e[idx].first_cluster);

                // fat16_list_dir_cluster(fs, e[idx].first_cluster);
            } else if (ext[0]) {
                // file with extension
                printf("[FILE] %s.%s size=%u cluster=%u", name, ext, e[idx].filesize, e[idx].first_cluster);
            } else {
                // file without extension
                printf("[FILE] %s size=%u cluster=%u", name, e[idx].filesize, e[idx].first_cluster);
            }
        }
    }
}

int fat16_find_path(
    fat16_fs_t* fs,
    const char* path,
    fat16_dir_entry_t* out
) {
    char part[13];
    uint16_t current_cluster = 0; // 0 = root
    const char* p = path;

    while (*p == '/') p++;

    while (*p) {
        int len = 0;
        while (p[len] && p[len] != '/') len++;

        memcpy(part, p, len);
        part[len] = 0;

        if (current_cluster == 0) {
            if (fat16_find_file(fs, part, out) != 0)
                return -1;
        } else {
            if (fat16_find_in_dir(fs, current_cluster, part, out) != 0)
                return -1;
        }

        p += len;
        while (*p == '/') p++;

        if (*p) {
            if (!(out->attr & 0x10))
                return -1; // not a directory
            current_cluster = out->first_cluster;
        }
    }

    return 0;
}

int fat16_match_name(fat16_dir_entry_t* e, const char* name) {
    char fatname[11];
    fat16_format_name(name, fatname);
    return memcmp(e->name, fatname, 11) == 0;
}

int fat16_find_in_dir(
    fat16_fs_t* fs,
    uint16_t current_cluster,
    const char* name,
    fat16_dir_entry_t* out
) {
    uint8_t buf[512];
    char fatname[11];
    fat16_format_name(name, fatname);

    // ---------- ROOT DIRECTORY ----------
    if (current_cluster == 0) {
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                int v = fat16_dir_valid(&e[j]);
                if (v == 0) return -1;
                if (v < 0) continue;

                if (memcmp(e[j].name, fatname, 11) == 0) {
                    *out = e[j];
                    return 0;
                }
            }
        }
        return -1;
    }

    // ---------- SUBDIRECTORY ----------
    uint16_t cluster = current_cluster;

    while (cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                int v = fat16_dir_valid(&e[j]);
                if (v == 0) return -1;
                if (v < 0) continue;

                if (memcmp(e[j].name, fatname, 11) == 0) {
                    *out = e[j];
                    return 0;
                }
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return -1;
}

void fat16_list_dir_cluster(fat16_fs_t* fs, uint16_t start_cluster) {
    uint8_t buf[512];
    uint16_t cluster = start_cluster;

    while (cluster < 0xFFF8) {
        uint32_t lba =
            fs->data_start +
            (cluster - 2) * fs->bs.sectors_per_cluster;

        for (int s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < 16; i++) {
                if (e[i].name[0] == 0x00) return;
                if (e[i].name[0] == 0xE5) continue;
                if (e[i].attr & 0x08) continue; // volume label

                char name[9], ext[4];
                memcpy(name, e[i].name, 8);
                memcpy(ext,  e[i].ext,  3);
                name[8] = ext[3] = 0;

                for (int k = 7; k >= 0 && name[k] == ' '; k--) name[k] = 0;
                for (int k = 2; k >= 0 && ext[k]  == ' '; k--) ext[k]  = 0;

                if (e[i].attr & 0x10){
                    printf("[DIR ] %s", name);
                } else if (ext[0])
                    printf("[FILE] %s.%s (%u bytes)", name, ext, e[i].filesize);
                else
                    printf("[FILE] %s (%u bytes)", name, e[i].filesize);
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }
}

void fat16_format_name(const char* input, char out[11]) {
    memset(out, ' ', 11);

    int i = 0, j = 0;

    // name
    while (input[i] && input[i] != '.' && j < 8) {
        out[j++] = toupper(input[i++]);
    }

    // extension
    if (input[i] == '.') {
        i++;
        j = 8;
        while (input[i] && j < 11) {
            out[j++] = toupper(input[i++]);
        }
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
            if (e[j].name[0] == 0x00) return -1;
            if (e[j].name[0] == 0xE5) continue;
            if (e[j].attr & 0x08) continue; // volume label

            if (memcmp(e[j].name, fatname, 11) == 0) {
                *out = e[j];
                return 0;
            }
        }
    }
    return -1;
}


void fat16_read_file(fat16_fs_t* fs, fat16_dir_entry_t* file) {
    uint16_t cluster = file->first_cluster;
    uint8_t buf[512];

    while (cluster < 0xFFF8) {
        uint32_t lba =
            fs->data_start +
            (cluster - 2) * fs->bs.sectors_per_cluster;

        for (int s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            for (int i = 0; i < 512; i++)
                printfnoln("%c", buf[i]);
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

    print("\n");
}

int fat16_open(fat16_fs_t* fs, const char* path, fat16_file_t* f) {
    uint16_t parent;
    char name[13];

    if (fat16_find_parent(fs, path, &parent, name) != 0)
        return -1;

    fat16_dir_entry_t entry;
    if (fat16_find_in_dir(fs, parent, name, &entry) != 0)
        return -1;

    if (entry.attr & 0x10)
        return -1; // directory

    f->fs = fs;
    f->entry = entry;
    f->parent_cluster = parent;
    f->pos = 0;
    f->cluster = entry.first_cluster;

    return 0;
}

int fat16_read(fat16_file_t* f, uint8_t* out, uint32_t size) {
    uint32_t read = 0;
    uint8_t sector[512];

    while (read < size && f->pos < f->entry.filesize) {
        uint32_t cluster_size =
            f->fs->bs.sectors_per_cluster * 512;

        uint32_t cluster_index = f->pos / cluster_size;
        uint32_t sector_in_cluster =
            (f->pos / 512) % f->fs->bs.sectors_per_cluster;

        uint16_t cluster = f->cluster;

        // Advance cluster only when needed
        while (cluster_index--) {
            cluster = fat16_read_fat_fs(f->fs, cluster);
        }

        uint32_t lba =
            fat16_cluster_lba(f->fs, cluster) + sector_in_cluster;

        ahci_read_sector(f->fs->portno, lba, sector, 1);

        uint32_t off = f->pos % 512;
        uint32_t to_copy = 512 - off;

        if (to_copy > size - read)
            to_copy = size - read;
        if (to_copy > f->entry.filesize - f->pos)
            to_copy = f->entry.filesize - f->pos;

        memcpy(out + read, sector + off, to_copy);

        f->pos += to_copy;
        read += to_copy;
        f->cluster = cluster;
    }

    return read;
}

int fat16_write(fat16_file_t* f, const uint8_t* data, uint32_t size) {
    uint32_t written = 0;
    uint8_t sector[512];

    uint32_t cluster_size =
        f->fs->bs.sectors_per_cluster * 512;

    /* ---------- allocate first cluster if empty ---------- */
    if (f->entry.first_cluster == 0) {
        uint16_t c = fat16_allocate_cluster(f->fs);
        if (!c) return 0;

        fat16_write_fat_entry(f->fs, c, FAT16_EOC);
        f->entry.first_cluster = c;
        f->cluster = c;
    }

    while (written < size) {
        uint32_t cluster_index = f->pos / cluster_size;
        uint32_t sector_in_cluster =
            (f->pos / 512) % f->fs->bs.sectors_per_cluster;

        uint16_t cluster = f->entry.first_cluster;

        /* ---------- walk / extend FAT chain ---------- */
        for (uint32_t i = 0; i < cluster_index; i++) {
            uint16_t next = fat16_read_fat_fs(f->fs, cluster);
            if (next >= FAT16_EOC) {
                next = fat16_allocate_cluster(f->fs);
                if (!next) return written;
                fat16_write_fat_entry(f->fs, cluster, next);
                fat16_write_fat_entry(f->fs, next, FAT16_EOC);
            }
            cluster = next;
        }

        uint32_t lba =
            fat16_cluster_lba(f->fs, cluster) + sector_in_cluster;

        ahci_read_sector(f->fs->portno, lba, sector, 1);

        uint32_t off = f->pos % 512;
        uint32_t to_copy = 512 - off;
        if (to_copy > size - written)
            to_copy = size - written;

        memcpy(sector + off, data + written, to_copy);
        ahci_write_sector(f->fs->portno, lba, sector, 1);

        f->pos += to_copy;
        written += to_copy;
    }

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
    uint32_t fat_sectors = fs->bs.sectors_per_fat;
    uint8_t buf[512];

    for (uint32_t s = 0; s < fat_sectors; s++) {
        ahci_read_sector(fs->portno, fs->fat_start + s, buf, 1);

        uint16_t* fat = (uint16_t*)buf;
        for (int i = 0; i < 256; i++) {
            if (fat[i] == 0x0000) {
                return s * 256 + i;
            }
        }
    }

    return 0; // no space
}

void fat16_write_fat_entry(fat16_fs_t* fs, uint16_t cluster, uint16_t value) {
    uint32_t offset = cluster * 2;
    uint32_t sector = fs->fat_start + offset / fs->bs.bytes_per_sector;
    uint32_t off = offset % fs->bs.bytes_per_sector;

    uint8_t buf[512];
    ahci_read_sector(fs->portno, sector, buf, 1);

    *(uint16_t*)(buf + off) = value;

    ahci_write_sector(fs->portno, sector, buf, 1);
}

uint16_t fat16_allocate_cluster(fat16_fs_t* fs) {
    uint16_t cluster = fat16_find_free_cluster(fs);
    if (!cluster)
        return 0;

    fat16_write_fat_entry(fs, cluster, FAT16_EOC);

    uint8_t zero[512] = {0};
    uint32_t lba = fat16_cluster_lba(fs, cluster);

    for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
        ahci_write_sector(fs->portno, lba + s, zero, 1);
    }

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

            if (e[j].name[0] == 0xE5)
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

void fat16_free_chain(fat16_fs_t* fs, uint16_t start_cluster) {
    uint16_t cluster = start_cluster;

    while (cluster >= 2 && cluster < FAT16_EOC) {
        uint16_t next = fat16_read_fat_fs(fs, cluster);
        fat16_write_fat_entry(fs, cluster, 0x0000); // mark free
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

    while (cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (memcmp(e[i].name, entry->name, 11) == 0) {
                    e[i] = *entry;
                    ahci_write_sector(fs->portno, lba + s, buf, 1);
                    return 0;
                }
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return -1;
}

int fat16_find_free_dir_entry(
    fat16_fs_t* fs,
    uint16_t dir_cluster,
    uint32_t* out_lba,
    uint32_t* out_index
) {
    uint8_t buf[512];
    uint16_t cluster = dir_cluster;

    while (1) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00 || e[i].name[0] == 0xE5) {
                    *out_lba = lba + s;
                    *out_index = i;
                    return 0;
                }
            }
        }

        uint16_t next = fat16_read_fat_fs(fs, cluster);
        if (next >= FAT16_EOC) {
            next = fat16_append_cluster(fs, cluster);
            if (!next) return -1;
        }
        cluster = next;
    }
}

int fat16_create(
    fat16_fs_t* fs,
    uint16_t parent_cluster, // 0 = root
    const char* name,
    uint8_t attr
) {
    fat16_dir_entry_t tmp;
    if (fat16_find_in_dir(fs, parent_cluster, name, &tmp) == 0) {
        printf("create: '%s' already exists", name);
        return -1;
    }

    fat16_dir_entry_t e;
    memset(&e, 0, sizeof(e));
    fat16_format_name(name, e.name);
    e.attr = attr;
    e.first_cluster = 0;
    e.filesize = 0;


    uint8_t buf[512];

    // ---------- ROOT ----------
    if (parent_cluster == 0) {
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* d = (fat16_dir_entry_t*)buf;

            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (d[j].name[0] == 0x00 || d[j].name[0] == 0xE5) {
                    d[j] = e;
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, buf, 1);
                    return 0;
                }
            }
        }
        return -1;
    }

    // ---------- SUBDIR ----------
    uint32_t lba, idx;
    if (fat16_find_free_dir_entry(fs, parent_cluster, &lba, &idx) != 0)
        return -1;

    ahci_read_sector(fs->portno, lba, buf, 1);
    ((fat16_dir_entry_t*)buf)[idx] = e;
    ahci_write_sector(fs->portno, lba, buf, 1);
    return 0;
}

int fat16_unlink(
    fat16_fs_t* fs,
    uint16_t parent_cluster,
    const char* name
) {

    fat16_dir_entry_t e;

    if (fat16_find_in_dir(fs, parent_cluster, name, &e) != 0) {
        printf("unlink: '%s' not found", name);
        return -1;
    }

    if (e.attr & 0x10) {
        printf("unlink: '%s' is a directory", name);
        return -1;
    }

    if (e.first_cluster)
        fat16_free_chain(fs, e.first_cluster);

    uint8_t buf[512];
    char fatname[11];
    fat16_format_name(name, fatname);

    if (parent_cluster == 0) {
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* d = (fat16_dir_entry_t*)buf;

            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(d[j].name, fatname, 11) == 0) {
                    d[j].name[0] = 0xE5;
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, buf, 1);
                    return 0;
                }
            }
        }
        return -1;
    }

    uint16_t cluster = parent_cluster;
    while (cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* d = (fat16_dir_entry_t*)buf;

            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(d[j].name, fatname, 11) == 0) {
                    d[j].name[0] = 0xE5;
                    ahci_write_sector(fs->portno, lba + s, buf, 1);
                    return 0;
                }
            }
        }
        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return -1;
}

int fat16_truncate(fat16_file_t* f, uint32_t new_size) {
    if (new_size >= f->entry.filesize)
        return 0;

    uint32_t cluster_size =
        f->fs->bs.sectors_per_cluster * 512;

    /* ----- truncate to zero ----- */
    if (new_size == 0) {
        if (f->entry.first_cluster >= 2)
            fat16_free_chain(f->fs, f->entry.first_cluster);

        f->entry.first_cluster = 0;
        f->entry.filesize = 0;

        goto update;
    }

    uint32_t keep_clusters =
        (new_size + cluster_size - 1) / cluster_size;

    uint16_t cluster = f->entry.first_cluster;
    uint16_t prev = 0;

    for (uint32_t i = 0; i < keep_clusters; i++) {
        prev = cluster;
        cluster = fat16_read_fat_fs(f->fs, cluster);
    }

    if (prev)
        fat16_write_fat_entry(f->fs, prev, FAT16_EOC);

    if (cluster < FAT16_EOC)
        fat16_free_chain(f->fs, cluster);

    f->entry.filesize = new_size;
    
update:
    if (f->parent_cluster == 0)
        fat16_update_root_entry(f->fs, &f->entry);
    else
        fat16_update_dir_entry(f->fs, f->parent_cluster, &f->entry);

    return 0;
}

int fat16_mkdir(fat16_fs_t* fs, uint16_t parent_cluster, const char* name) {
    uint16_t new_cluster = fat16_allocate_cluster(fs);
    if (!new_cluster) return -1;

    fat16_dir_entry_t tmp;
    if (fat16_find_in_dir(fs, parent_cluster, name, &tmp) == 0) {
        printf("mkdir: directory '%s' already exists", name);
        return -1;
    }

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
                if (d[j].name[0] == 0x00 || d[j].name[0] == 0xE5) {
                    d[j] = dir;
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, sector, 1);
                    return 0;
                }
            }
        }
        return -1;
    }

    if (fat16_find_free_dir_entry(fs, parent_cluster, &out_lba, &out_idx) != 0)
        return -1;

    ahci_read_sector(fs->portno, out_lba, sector, 1);
    ((fat16_dir_entry_t*)sector)[out_idx] = dir;
    ahci_write_sector(fs->portno, out_lba, sector, 1);

    return 0;
}

static int fat16_dir_is_empty(fat16_fs_t* fs, uint16_t cluster) {
    uint8_t buf[512];

    while (cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00)
                    return 1;

                if (e[i].name[0] == 0xE5)
                    continue;

                // skip . and ..
                if (e[i].name[0] == '.' )
                    continue;

                return 0; // found real entry
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return 1;
}

int fat16_create_path(fat16_fs_t* fs, const char* path, uint8_t attr) {
    char part[13];
    uint16_t current_cluster = 0;
    const char* p = path;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        int len = 0;
        while (p[len] && p[len] != '/') len++;
        memcpy(part, p, len);
        part[len] = 0;

        fat16_dir_entry_t entry;
        int is_last = (p[len] == '\0');

        if (fat16_find_in_dir(fs, current_cluster, part, &entry) != 0) {
            // does not exist → create
            if (!is_last) {
                if (fat16_mkdir(fs, current_cluster, part) != 0)
                    return -1;
            } else {
                if (fat16_create(fs, current_cluster, part, attr) != 0)
                    return -1;
                return 0;
            }
        } else {
            // already exists
            if (is_last) {
                printf("create: '%s' already exists", part);
                return -1;
            }
        }

        if (!is_last) {
            if (fat16_find_in_dir(fs, current_cluster, part, &entry) != 0)
                return -1;
            current_cluster = entry.first_cluster;
        }

        p += len;
    }

    return 0;
}

int fat16_find_parent(fat16_fs_t* fs, const char* path, uint16_t* out_cluster, char* out_name) {
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) return -1;

    // Copy filename
    strcpy(out_name, last_slash + 1);

    // Handle root
    if (last_slash == path) {
        *out_cluster = 0;
        return 0;
    }

    // Copy parent path
    char parent_path[128];
    int len = last_slash - path;
    strncpy(parent_path, path, len);
    parent_path[len] = 0;

    fat16_dir_entry_t entry;
    if (fat16_find_path(fs, parent_path, &entry) != 0) return -1;

    *out_cluster = entry.first_cluster;
    return 0;
}

int fat16_delete_entry(fat16_fs_t* fs, uint16_t parent_cluster, const char* name) {
    fat16_dir_entry_t e;
    if (fat16_find_in_dir(fs, parent_cluster, name, &e) != 0) return -1;

    if (e.first_cluster >= 2)
        fat16_free_chain(fs, e.first_cluster);

    uint8_t buf[512];
    char fatname[11];
    fat16_format_name(name, fatname);

    // Root directory
    if (parent_cluster == 0) {
        for (uint32_t i = 0; i < fs->root_dir_sectors; i++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + i, buf, 1);
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(entries[j].name, fatname, 11) == 0) {
                    entries[j].name[0] = 0xE5;
                    ahci_write_sector(fs->portno, fs->root_dir_start + i, buf, 1);
                    return 0;
                }
            }
        }
        return -1;
    }

    // Subdirectory
    uint16_t cluster = parent_cluster;
    while (cluster < FAT16_EOC) {
        uint32_t lba = fat16_cluster_lba(fs, cluster);
        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(fs->portno, lba + s, buf, 1);
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
            for (int j = 0; j < DIR_ENTRIES_PER_SECTOR; j++) {
                if (memcmp(entries[j].name, fatname, 11) == 0) {
                    entries[j].name[0] = 0xE5;
                    ahci_write_sector(fs->portno, lba + s, buf, 1);
                    return 0;
                }
            }
        }
        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return -1;
}

void fat16_unformat_name(const fat16_dir_entry_t* e, char* out) {
    int i = 0;
    // Copy name
    for (int j = 0; j < 8 && e->name[j] != ' '; j++)
        out[i++] = e->name[j];

    // Copy extension if present
    if (e->ext[0] != ' ') {
        out[i++] = '.';
        for (int j = 0; j < 3 && e->ext[j] != ' '; j++)
            out[i++] = e->ext[j];
    }
    out[i] = 0;
}

int fat16_ls(fat16_fs_t* fs, const char* path) {
    fat16_dir_entry_t dir;

    // Resolve directory
    if (strcmp(path, "/") == 0) {
        dir.first_cluster = 0;
    } else {
        if (fat16_find_path(fs, path, &dir) != 0) {
            printf("ls: cannot access '%s'", path);
            return -1;
        }

        if (!(dir.attr & 0x10)) {
            printf("ls: '%s' is not a directory", path);
            return -1;
        }
    }

    uint8_t buf[512];

    // -------- ROOT DIRECTORY --------
    if (dir.first_cluster == 0) {
        for (uint32_t s = 0; s < fs->root_dir_sectors; s++) {
            ahci_read_sector(fs->portno, fs->root_dir_start + s, buf, 1);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00) return 0;
                if (e[i].name[0] == 0xE5) continue;

                char name[13];
                fat16_unformat_name(e[i].name, name);

                printf("%s%s\n",
                       name,
                       (e[i].attr & 0x10) ? "/" : "");
            }
        }
        return 0;
    }

    // -------- SUBDIRECTORY --------
    uint16_t cluster = dir.first_cluster;

    while (cluster < FAT16_EOC) {
        for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; s++) {
            ahci_read_sector(
                fs->portno,
                fat16_cluster_lba(fs, cluster) + s,
                buf,
                1
            );

            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;

            for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; i++) {
                if (e[i].name[0] == 0x00) return 0;
                if (e[i].name[0] == 0xE5) continue;

                char name[13];
                fat16_unformat_name(e[i].name, name);

                printf("%s%s",name, (e[i].attr & 0x10) ? "/" : "");
            }
        }

        cluster = fat16_read_fat_fs(fs, cluster);
    }

    return 0;
}