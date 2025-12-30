/**
 * @file fat16.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The main code for FAT16 Read & Write.
 * @version 0.1
 * @date 2025-12-28
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <filesystems/fat16.h>

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

    done("Successfully mounted FAT16 parition",  __FILE__);
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
    if (fat16_find_path(fs, path, &f->entry) != 0)
        return -1;

    if (f->entry.attr & 0x10)
        return -1; // directory

    f->fs = fs;
    f->pos = 0;
    f->cluster = f->entry.first_cluster;
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

    while (written < size) {
        uint32_t cluster_size =
            f->fs->bs.sectors_per_cluster * 512;

        uint32_t cluster_index = f->pos / cluster_size;
        uint32_t sector_in_cluster =
            (f->pos / 512) % f->fs->bs.sectors_per_cluster;

        uint16_t cluster = f->cluster;

        // Walk / extend FAT chain
        for (uint32_t i = 0; i < cluster_index; i++) {
            uint16_t next = fat16_read_fat_fs(f->fs, cluster);
            if (next >= FAT16_EOC) {
                next = fat16_append_cluster(f->fs, cluster);
                if (!next) return written;
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

        if (f->pos > f->entry.filesize)
            f->entry.filesize = f->pos;

        f->cluster = cluster;
    }

    fat16_update_root_entry(f->fs, &f->entry);
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
