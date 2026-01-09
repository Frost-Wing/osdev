/**
 * @file fat32.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief fat32 code bro else what? Will i put feet pics in a file named fat32.c?
 * @version 0.1
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#include <filesystems/fat32.h>
#include <strings.h>
#include <basics.h>
#include <graphics.h>

/* ========================== */
/*  LOW LEVEL DISK WRAPPERS   */
/* ========================== */

static inline int fat32_read_sector(fat32_fs_t* fs, uint32_t lba, void* buf) {
    return ahci_read_sector(fs->portno, lba, buf, 1);
}

static inline int fat32_write_sector(fat32_fs_t* fs, uint32_t lba, const void* buf) {
    return ahci_write_sector(fs->portno, lba, buf, 1);
}

/* ========================== */
/*  CLUSTER / FAT UTILITIES   */
/* ========================== */

static inline uint32_t fat32_cluster_lba(fat32_fs_t* fs, uint32_t cluster) {
    return fs->data_start_lba +
           (cluster - 2) * fs->sectors_per_cluster;
}

uint32_t fat32_read_fat(fat32_fs_t* fs, uint32_t cluster) {
    uint32_t offset = cluster * 4;
    uint32_t lba = fs->fat_start_lba + (offset / FAT32_SECTOR_SIZE);
    uint32_t off = offset % FAT32_SECTOR_SIZE;

    uint8_t sector[FAT32_SECTOR_SIZE];
    if (fat32_read_sector(fs, lba, sector))
        return FAT32_CLUSTER_EOC;

    return (*(uint32_t*)(sector + off)) & 0x0FFFFFFF;
}

static int fat32_write_fat(fat32_fs_t* fs, uint32_t cluster, uint32_t value) {
    uint32_t offset = cluster * 4;
    uint32_t lba = fs->fat_start_lba + (offset / FAT32_SECTOR_SIZE);
    uint32_t off = offset % FAT32_SECTOR_SIZE;

    uint8_t sector[FAT32_SECTOR_SIZE];
    if (fat32_read_sector(fs, lba, sector))
        return -1;

    *(uint32_t*)(sector + off) = value;
    return fat32_write_sector(fs, lba, sector);
}

uint32_t fat32_alloc_cluster(fat32_fs_t* fs) {
    for (uint32_t c = 2; c < fs->total_clusters + 2; c++) {
        if (fat32_read_fat(fs, c) == FAT32_CLUSTER_FREE) {
            fat32_write_fat(fs, c, FAT32_CLUSTER_EOC);
            return c;
        }
    }
    return 0;
}

static void fat32_free_chain(fat32_fs_t* fs, uint32_t cluster) {
    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
        uint32_t next = fat32_read_fat(fs, cluster);
        fat32_write_fat(fs, cluster, FAT32_CLUSTER_FREE);
        cluster = next;
    }
}

int fat32_match_name(const fat32_dir_entry_t* e, const char* name)
{
    if (!e || !name)
        return 0;

    char fat_name[13];
    int p = 0;

    /* Base name */
    for (int i = 0; i < 8 && e->name[i] != ' '; i++)
        fat_name[p++] = e->name[i];

    /* Extension */
    if (e->name[8] != ' ') {
        fat_name[p++] = '.';
        for (int i = 8; i < 11 && e->name[i] != ' '; i++)
            fat_name[p++] = e->name[i];
    }

    fat_name[p] = '\0';

    /* CASE-SENSITIVE compare */
    return strcmp(fat_name, name) == 0;
}

/* ========================== */
/*  CLUSTER READ              */
/* ========================== */

static int fat32_read_cluster(fat32_fs_t* fs, uint32_t cluster, uint8_t* buf) {
    uint32_t lba = fat32_cluster_lba(fs, cluster);
    for (uint32_t i = 0; i < fs->sectors_per_cluster; i++) {
        if (fat32_read_sector(fs, lba + i,
            buf + i * FAT32_SECTOR_SIZE))
            return -1;
    }
    return 0;
}

/* ========================== */
/*  MOUNT                     */
/* ========================== */

int fat32_mount(int portno, uint32_t part_lba, fat32_fs_t* fs) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    if (!fs) return -1;
    if (ahci_read_sector(portno, part_lba, sector, 1)) return -1;

    memcpy(&fs->bpb, sector, sizeof(fat32_bpb_t));

    if (fs->bpb.bytes_per_sector != FAT32_SECTOR_SIZE)
        return -1;

    fs->portno = portno;
    fs->partition_lba = part_lba;

    fs->fat_start_lba = part_lba + fs->bpb.reserved_sectors;
    fs->data_start_lba = fs->fat_start_lba +
                         fs->bpb.fat_count * fs->bpb.fat_size_32;

    fs->sectors_per_cluster = fs->bpb.sectors_per_cluster;
    fs->root_cluster = fs->bpb.root_cluster;

    uint32_t total = fs->bpb.total_sectors_32;
    uint32_t data = total - (fs->bpb.reserved_sectors +
                             fs->bpb.fat_count * fs->bpb.fat_size_32);

    fs->total_clusters = data / fs->sectors_per_cluster;
    return 0;
}

/* ========================== */
/*  OPEN (LFN + 8.3)          */
/* ========================== */

int fat32_open(fat32_fs_t* fs, const char* path, fat32_file_t* f) {
    if (!fs || !path || !f) return -1;

    uint32_t cluster = fs->root_cluster;
    char tmp[256];
    strncpy(tmp, path, sizeof(tmp));

    char* tok = strtok(tmp, "/");
    fat32_dir_entry_t found = {0};

    while (tok) {
        uint8_t buf[fs->sectors_per_cluster * FAT32_SECTOR_SIZE];
        int hit = 0;

        while (cluster < FAT32_CLUSTER_EOC) {
            fat32_read_cluster(fs, cluster, buf);

            for (uint32_t off = 0; off < sizeof(buf); off += 32) {
                fat32_dir_entry_t* e = (void*)(buf + off);
                if (e->name[0] == 0x00) break;
                if (e->attr == FAT_ATTR_LFN) continue;

                char name[13] = {0};
                memcpy(name, e->name, 11);

                if (!strcasecmp(name, tok)) {
                    found = *e;
                    cluster = (e->first_cluster_high << 16) |
                               e->first_cluster_low;
                    hit = 1;
                    break;
                }
            }
            if (hit) break;
            cluster = fat32_read_fat(fs, cluster);
        }

        if (!hit) return -1;
        tok = strtok(NULL, "/");
    }

    memset(f, 0, sizeof(*f));
    f->fs = fs;
    f->entry = found;
    f->start_cluster =
        (found.first_cluster_high << 16) |
         found.first_cluster_low;
    f->current_cluster = f->start_cluster;
    f->pos = 0;
    return 0;
}

/* ========================== */
/*  READ                      */
/* ========================== */

int fat32_read(fat32_file_t* f, void* buf, uint32_t len) {
    if (!f || !buf) return -1;

    uint8_t* out = buf;
    uint32_t done = 0;
    uint32_t cluster_size =
        f->fs->sectors_per_cluster * FAT32_SECTOR_SIZE;

    while (len && f->pos < f->entry.file_size) {
        uint8_t clbuf[cluster_size];
        fat32_read_cluster(f->fs, f->current_cluster, clbuf);

        uint32_t off = f->pos % cluster_size;
        uint32_t take = cluster_size - off;
        if (take > len) take = len;
        if (take > f->entry.file_size - f->pos)
            take = f->entry.file_size - f->pos;

        memcpy(out + done, clbuf + off, take);

        done += take;
        f->pos += take;
        len -= take;

        if (off + take >= cluster_size)
            f->current_cluster = fat32_read_fat(f->fs, f->current_cluster);
    }
    return done;
}

/* ========================== */
/*  WRITE (AUTO EXTEND)       */
/* ========================== */

int fat32_write(fat32_file_t* f, const void* buf, uint32_t len) {
    if (!f || !buf) return -1;

    const uint8_t* in = buf;
    uint32_t done = 0;
    uint32_t cluster_size =
        f->fs->sectors_per_cluster * FAT32_SECTOR_SIZE;

    if (!f->start_cluster) {
        f->start_cluster = fat32_alloc_cluster(f->fs);
        f->current_cluster = f->start_cluster;
    }

    while (len) {
        uint8_t clbuf[cluster_size];
        fat32_read_cluster(f->fs, f->current_cluster, clbuf);

        uint32_t off = f->pos % cluster_size;
        uint32_t take = cluster_size - off;
        if (take > len) take = len;

        memcpy(clbuf + off, in + done, take);

        uint32_t lba = fat32_cluster_lba(f->fs, f->current_cluster);
        for (uint32_t i = 0; i < f->fs->sectors_per_cluster; i++)
            fat32_write_sector(f->fs, lba + i,
                clbuf + i * FAT32_SECTOR_SIZE);

        done += take;
        f->pos += take;
        len -= take;

        if (off + take >= cluster_size) {
            uint32_t next = fat32_read_fat(f->fs, f->current_cluster);
            if (next >= FAT32_CLUSTER_EOC) {
                next = fat32_alloc_cluster(f->fs);
                fat32_write_fat(f->fs, f->current_cluster, next);
            }
            f->current_cluster = next;
        }
    }

    if (f->pos > f->entry.file_size)
        f->entry.file_size = f->pos;

    return done;
}

/* ========================== */
/*  CLOSE                     */
/* ========================== */

void fat32_close(fat32_file_t* f) {
    if (f) memset(f, 0, sizeof(*f));
}

void fat32_list_root(fat32_fs_t* fs)
{
    uint32_t root_cluster = fs->bpb.root_cluster;
    fat32_list_dir_cluster(fs, root_cluster);
}

void fat32_list_dir_cluster(fat32_fs_t* fs, uint32_t cluster)
{
    uint32_t entries_per_cluster = (fs->sectors_per_cluster * FAT32_SECTOR_SIZE) / sizeof(fat32_dir_entry_t);
    fat32_dir_entry_t* entries = kmalloc(entries_per_cluster * sizeof(fat32_dir_entry_t));

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC)
    {
        fat32_read_cluster(fs, cluster, entries);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            fat32_dir_entry_t* e = &entries[i];

            if (e->name[0] == 0x00)
                return;

            if (e->name[0] == 0xE5)
                continue;

            if (e->attr == FAT_ATTR_LFN)
                continue;

            fat32_print_entry(e);
        }

        cluster = fat32_read_fat(fs, cluster);
    }
}

int fat32_find_path(fat32_fs_t* fs, const char* path, fat32_dir_entry_t* out)
{
    if (!path || !*path) return FAT_ERR_INVALID;

    char name[12];
    uint32_t cluster = fs->bpb.root_cluster;

    const char* p = path;

    while (*p) {
        p = fat_next_path_component(p, name);

        if (!fat32_find_in_dir(fs, cluster, name, out))
            return FAT_ERR_NOT_FOUND;

        cluster = ((uint32_t)out->first_cluster_high << 16)
                | out->first_cluster_low;
    }

    return FAT_OK;
}

int fat32_create_path(fat32_fs_t* fs, const char* path, fat32_dir_entry_t* out)
{
    char parent[256];
    char name[12];

    fat_split_path(path, parent, name);

    fat32_dir_entry_t dir;
    uint32_t dir_cluster;

    if (*parent == '\0') {
        dir_cluster = fs->bpb.root_cluster;
    } else {
        if (fat32_find_path(fs, parent, &dir) != FAT_OK)
            return FAT_ERR_NOT_FOUND;

        dir_cluster = ((uint32_t)dir.first_cluster_high << 16)
                    | dir.first_cluster_low;
    }

    return fat32_create_entry(fs, dir_cluster, name, FAT_ATTR_ARCHIVE, out);
}

int fat32_truncate(fat32_file_t* file, uint32_t new_size)
{
    if (!file || !file->fs)
        return FAT_ERR_INVALID;

    fat32_fs_t* fs = file->fs;
    fat32_dir_entry_t* entry = &file->entry;

    uint32_t old_clusters =
        fat32_clusters_for_size(fs, entry->file_size);

    uint32_t new_clusters =
        fat32_clusters_for_size(fs, new_size);

    uint32_t start_cluster =
        ((uint32_t)entry->first_cluster_high << 16) |
         entry->first_cluster_low;

    /* Shrink */
    if (new_clusters < old_clusters) {
        fat32_free_chain_from(fs, start_cluster, new_clusters);
    }
    /* Grow */
    else if (new_clusters > old_clusters) {
        fat32_extend_chain(
            fs,
            start_cluster,
            new_clusters - old_clusters
        );
    }

    entry->file_size = new_size;

    fat32_update_entry(fs, entry);

    return FAT_OK;
}

void fat32_print_entry(const fat32_dir_entry_t* e)
{
    if (e->name[0] == 0x00 || e->name[0] == 0xE5)
        return;

    if (e->attr & FAT_ATTR_VOLUME_ID)
        return;

    char name[12];
    memcpy(name, e->name, 11);
    name[11] = 0;

    printf("%s%s" reset_color, (e->attr & FAT_ATTR_DIRECTORY) ? yellow_color : blue_color, name);
}

const char* fat_next_path_component(const char* path, char* out)
{
    while (*path == '/')
        path++;

    if (!*path)
        return NULL;

    int i = 0;
    while (*path && *path != '/' && i < 11)
        out[i++] = *path++;

    out[i] = 0;
    return path;
}

void fat_split_path(const char* path, char* parent, char* name)
{
    const char* last = path;
    const char* p = path;

    while (*p) {
        if (*p == '/')
            last = p;
        p++;
    }

    if (last != path) {
        memcpy(parent, path, last - path);
        parent[last - path] = 0;
        strcpy(name, last + 1);
    } else {
        parent[0] = '/';
        parent[1] = 0;
        strcpy(name, path);
    }
}

int fat32_find_in_dir(
    fat32_fs_t* fs,
    uint32_t cluster,
    const char* name,
    fat32_dir_entry_t* out)
{
    uint32_t entries_per_cluster =
        (fs->sectors_per_cluster * FAT32_SECTOR_SIZE) /
        sizeof(fat32_dir_entry_t);

    fat32_dir_entry_t* entries = kmalloc(entries_per_cluster * sizeof(fat32_dir_entry_t));

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
        fat32_read_cluster(fs, cluster, entries);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00)
                return -1;

            if (memcmp(entries[i].name, name, 11) == 0) {
                *out = entries[i];
                return 0;
            }
        }

        cluster = fat32_read_fat(fs, cluster);
    }

    return -1;
}

uint32_t fat32_clusters_for_size(fat32_fs_t* fs, uint32_t size)
{
    uint32_t cluster_size =
        fs->sectors_per_cluster * FAT32_SECTOR_SIZE;

    return (size + cluster_size - 1) / cluster_size;
}

void fat32_update_entry(fat32_fs_t* fs, fat32_dir_entry_t* e)
{
    (void)fs;
    (void)e;
}

void fat32_free_chain_from(
    fat32_fs_t* fs,
    uint32_t start,
    uint32_t keep)
{
    uint32_t c = start;

    while (keep-- && c < FAT32_CLUSTER_EOC)
        c = fat32_read_fat(fs, c);

    while (c < FAT32_CLUSTER_EOC) {
        uint32_t next = fat32_read_fat(fs, c);
        fat32_write_fat(fs, c, FAT32_CLUSTER_FREE);
        c = next;
    }
}

void fat32_extend_chain(
    fat32_fs_t* fs,
    uint32_t start,
    uint32_t count)
{
    uint32_t c = start;

    while (fat32_read_fat(fs, c) < FAT32_CLUSTER_EOC)
        c = fat32_read_fat(fs, c);

    while (count--) {
        uint32_t n = fat32_alloc_cluster(fs);
        fat32_write_fat(fs, c, n);
        fat32_write_fat(fs, n, FAT32_CLUSTER_EOC);
        c = n;
    }
}

int fat32_create_entry(
    fat32_fs_t* fs,
    uint32_t dir_cluster,
    const char* name,
    uint8_t attr,
    fat32_dir_entry_t* out)
{
    uint32_t entries_per_cluster =
        (fs->sectors_per_cluster * FAT32_SECTOR_SIZE) /
        sizeof(fat32_dir_entry_t);

    fat32_dir_entry_t* entries = kmalloc(entries_per_cluster * sizeof(fat32_dir_entry_t));

    while (dir_cluster >= 2 && dir_cluster < FAT32_CLUSTER_EOC) {
        fat32_read_cluster(fs, dir_cluster, entries);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00 ||
                entries[i].name[0] == 0xE5)
            {
                memset(&entries[i], 0, sizeof(entries[i]));
                memcpy(entries[i].name, name, strlen(name));
                entries[i].attr = attr;

                uint32_t c = fat32_alloc_cluster(fs);
                entries[i].first_cluster_low  = c & 0xFFFF;
                entries[i].first_cluster_high = c >> 16;

                uint32_t lba = fat32_cluster_lba(fs, dir_cluster);
                for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
                    fat32_write_sector(
                        fs,
                        lba + s,
                        ((uint8_t*)entries) + s * FAT32_SECTOR_SIZE
                    );
                }

                if (out) *out = entries[i];
                return FAT_OK;
            }
        }

        dir_cluster = fat32_read_fat(fs, dir_cluster);
    }

    return FAT_ERR_NO_SPACE;
}

int fat32_unlink_path(fat32_fs_t* fs, const char* path)
{
    if (!fs || !path || !*path)
        return FAT_ERR_INVALID;

    char parent_path[256];
    char name[256];

    const char* last = strrchr(path, '/');
    if (last) {
        size_t len = last - path;
        memcpy(parent_path, path, len);
        parent_path[len] = 0;
        strncpy(name, last + 1, sizeof(name));
    } else {
        parent_path[0] = 0;
        strncpy(name, path, sizeof(name));
    }

    fat32_dir_entry_t parent;
    uint32_t parent_cluster = fs->root_cluster;

    if (parent_path[0]) {
        if (fat32_find_path(fs, parent_path, &parent) != FAT_OK)
            return FAT_ERR_NOT_FOUND;

        parent_cluster =
            ((uint32_t)parent.first_cluster_high << 16) |
             parent.first_cluster_low;
    }

    fat32_dir_entry_t e;
    if (fat32_find_in_dir(fs, parent_cluster, name, &e) != FAT_OK)
        return FAT_ERR_NOT_FOUND;

    if (e.attr & 0x10)
        return FAT_ERR_IS_DIR;

    uint32_t start =
        ((uint32_t)e.first_cluster_high << 16) |
         e.first_cluster_low;

    if (start >= 2)
        fat32_free_chain_from(fs, start, 0);

    fat32_delete_entry(fs, parent_cluster, name);
    return FAT_OK;
}

int fat32_rm_recursive(fat32_fs_t* fs, const char* path)
{
    if (!fs || !path || !*path)
        return FAT_ERR_INVALID;

    fat32_dir_entry_t e;
    if (fat32_find_path(fs, path, &e) != FAT_OK)
        return FAT_ERR_NOT_FOUND;

    uint32_t cluster =
        ((uint32_t)e.first_cluster_high << 16) |
         e.first_cluster_low;

    if (e.attr & 0x10) {
        /* directory */
        fat32_rmdir(fs, cluster);
    } else {
        /* file */
        if (cluster >= 2)
            fat32_free_chain_from(fs, cluster, 0);
    }

    /* delete entry itself */
    return fat32_unlink_path(fs, path);
}

int fat32_mv(fat32_fs_t* fs, const char* src, const char* dst)
{
    if (!fs || !src || !dst)
        return FAT_ERR_INVALID;

    /* ---------- find source entry ---------- */
    fat32_dir_entry_t src_entry;
    if (fat32_find_path(fs, src, &src_entry) != FAT_OK)
        return FAT_ERR_NOT_FOUND;

    /* ---------- split destination ---------- */
    char dst_parent[256];
    char dst_name[12];

    fat_split_path(dst, dst_parent, dst_name);

    uint32_t dst_dir_cluster;

    if (*dst_parent == '\0' || strcmp(dst_parent, "/") == 0) {
        dst_dir_cluster = fs->root_cluster;
    } else {
        fat32_dir_entry_t parent;
        if (fat32_find_path(fs, dst_parent, &parent) != FAT_OK)
            return FAT_ERR_NOT_FOUND;

        dst_dir_cluster =
            ((uint32_t)parent.first_cluster_high << 16) |
             parent.first_cluster_low;
    }

    /* ---------- create new entry ---------- */
    fat32_dir_entry_t new_entry;
    int ret = fat32_create_entry(
        fs,
        dst_dir_cluster,
        dst_name,
        src_entry.attr,
        &new_entry
    );

    if (ret != FAT_OK)
        return ret;

    /* ---------- copy cluster + size ---------- */
    new_entry.first_cluster_low  = src_entry.first_cluster_low;
    new_entry.first_cluster_high = src_entry.first_cluster_high;
    new_entry.file_size          = src_entry.file_size;

    fat32_update_entry(fs, &new_entry);

    /* ---------- delete old entry ONLY ---------- */
    char src_parent[256];
    char src_name[12];
    fat_split_path(src, src_parent, src_name);

    uint32_t src_dir_cluster = fs->root_cluster;

    if (*src_parent && strcmp(src_parent, "/") != 0) {
        fat32_dir_entry_t parent;
        if (fat32_find_path(fs, src_parent, &parent) != FAT_OK)
            return FAT_ERR_NOT_FOUND;

        src_dir_cluster =
            ((uint32_t)parent.first_cluster_high << 16) |
             parent.first_cluster_low;
    }

    fat32_delete_entry(fs, src_dir_cluster, src_name);

    return FAT_OK;
}

int fat32_delete_entry(fat32_fs_t* fs, uint32_t dir_cluster, const char* name)
{
    if (!fs || !name)
        return FAT_ERR_INVALID;

    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t cluster = dir_cluster;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
        uint32_t lba = fat32_cluster_lba(fs, cluster);

        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            fat32_read_sector(fs, lba + s, sector);

            fat32_dir_entry_t* e = (fat32_dir_entry_t*)sector;

            for (uint32_t i = 0; i < FAT32_SECTOR_SIZE / sizeof(fat32_dir_entry_t); i++) {
                if (e[i].name[0] == 0x00)
                    return FAT_ERR_NOT_FOUND;

                if (e[i].name[0] == 0xE5)
                    continue;

                if (fat32_match_name(&e[i], name)) {
                    e[i].name[0] = 0xE5; // mark deleted
                    fat32_write_sector(fs, lba + s, sector);
                    return FAT_OK;
                }
            }
        }

        cluster = fat32_read_fat(fs, cluster);
    }

    return FAT_ERR_NOT_FOUND;
}

int fat32_rmdir(fat32_fs_t* fs, fat32_dir_entry_t* entry)
{
    if (!fs || !entry)
        return FAT_ERR_INVALID;

    uint32_t cluster =
        ((uint32_t)entry->first_cluster_high << 16) |
         entry->first_cluster_low;

    uint8_t sector[FAT32_SECTOR_SIZE];

    uint32_t cur = cluster;
    while (cur >= 2 && cur < FAT32_CLUSTER_EOC) {
        uint32_t lba = fat32_cluster_lba(fs, cur);

        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            fat32_read_sector(fs, lba + s, sector);
            fat32_dir_entry_t* e = (fat32_dir_entry_t*)sector;

            for (uint32_t i = 0; i < FAT32_SECTOR_SIZE / sizeof(fat32_dir_entry_t); i++) {
                if (e[i].name[0] == 0x00)
                    break;

                if (e[i].name[0] == 0xE5)
                    continue;

                /* allow . and .. */
                if ((e[i].name[0] == '.') &&
                    (e[i].name[1] == ' ' || e[i].name[1] == '.'))
                    continue;

                return FAT_ERR_NOT_EMPTY;
            }
        }

        cur = fat32_read_fat(fs, cur);
    }

    fat32_free_chain(fs, cluster);
    return FAT_OK;
}
