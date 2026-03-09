#include <filesystems/iso9660.h>
#include <strings.h>
#include <heap.h>
#include <graphics.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t length;
    uint8_t ext_attr_length;
    uint32_t extent_lba_le;
    uint32_t extent_lba_be;
    uint32_t data_len_le;
    uint32_t data_len_be;
    uint8_t recording_time[7];
    uint8_t flags;
    uint8_t file_unit_size;
    uint8_t interleave_gap_size;
    uint16_t volume_seq_le;
    uint16_t volume_seq_be;
    uint8_t name_len;
    char name[1];
} iso9660_dir_record_t;
#pragma pack(pop)

static int iso_read_block(iso9660_fs_t* fs, uint32_t block, uint8_t* out)
{
    uint32_t lba = fs->partition_lba + block * (fs->logical_block_size / SECTOR_SIZE);
    uint32_t sectors = fs->logical_block_size / SECTOR_SIZE;
    return ahci_read_sector(fs->portno, lba, out, sectors);
}

static void iso_name_to_vfs(const char* in, uint8_t in_len, char* out, size_t out_sz)
{
    size_t oi = 0;

    for (uint8_t i = 0; i < in_len && oi + 1 < out_sz; i++) {
        char c = in[i];

        if (c == ';')
            break;

        if (c >= 'a' && c <= 'z')
            c = c - ('a' - 'A');

        out[oi++] = c;
    }

    if (oi > 0 && out[oi - 1] == '.')
        oi--;

    out[oi] = '\0';
}

static int iso_name_eq(const char* query, const char* rec_name, uint8_t rec_len)
{
    char lhs[128];
    char rhs[128];

    memset(lhs, 0, sizeof(lhs));
    memset(rhs, 0, sizeof(rhs));

    strncpy(lhs, query, sizeof(lhs) - 1);
    for (size_t i = 0; lhs[i]; i++) {
        if (lhs[i] >= 'a' && lhs[i] <= 'z')
            lhs[i] = lhs[i] - ('a' - 'A');
    }

    iso_name_to_vfs(rec_name, rec_len, rhs, sizeof(rhs));
    return strcmp(lhs, rhs) == 0;
}

static int iso_find_in_dir(iso9660_fs_t* fs,
                           uint32_t dir_extent,
                           uint32_t dir_size,
                           const char* name,
                           iso9660_dirent_t* out)
{
    if (!fs || !name || !out)
        return -1;

    uint8_t* blk = kmalloc(fs->logical_block_size);
    if (!blk)
        return -2;

    uint32_t bytes_seen = 0;
    uint32_t blocks = (dir_size + fs->logical_block_size - 1) / fs->logical_block_size;

    for (uint32_t b = 0; b < blocks; b++) {
        if (iso_read_block(fs, dir_extent + b, blk) != 0) {
            kfree(blk);
            return -3;
        }

        uint32_t off = 0;
        while (off < fs->logical_block_size && bytes_seen < dir_size) {
            iso9660_dir_record_t* r = (iso9660_dir_record_t*)(blk + off);
            if (r->length == 0)
                break;

            if (r->name_len == 1 && (uint8_t)r->name[0] <= 1) {
                off += r->length;
                bytes_seen += r->length;
                continue;
            }

            if (iso_name_eq(name, r->name, r->name_len)) {
                out->extent_lba = r->extent_lba_le;
                out->size = r->data_len_le;
                out->flags = r->flags;
                kfree(blk);
                return 0;
            }

            off += r->length;
            bytes_seen += r->length;
        }

        bytes_seen = (b + 1) * fs->logical_block_size;
    }

    kfree(blk);
    return -4;
}

int iso9660_detect_at_lba(int portno, uint32_t lba_start)
{
    uint8_t pvd[ISO9660_SECTOR_SIZE];

    if (ahci_read_sector(portno, lba_start + 16 * (ISO9660_SECTOR_SIZE / SECTOR_SIZE),
                         pvd, ISO9660_SECTOR_SIZE / SECTOR_SIZE) != 0)
        return 0;

    if (pvd[0] != 1)
        return 0;

    if (memcmp(&pvd[1], "CD001", 5) != 0)
        return 0;

    if (pvd[6] != 1)
        return 0;

    return 1;
}

int iso9660_mount(int portno, uint32_t partition_lba, iso9660_fs_t* fs)
{
    if (!fs)
        return -1;

    uint8_t pvd[ISO9660_SECTOR_SIZE];
    uint32_t pvd_lba = partition_lba + 16 * (ISO9660_SECTOR_SIZE / SECTOR_SIZE);

    if (ahci_read_sector(portno, pvd_lba, pvd, ISO9660_SECTOR_SIZE / SECTOR_SIZE) != 0)
        return -2;

    if (pvd[0] != 1 || memcmp(&pvd[1], "CD001", 5) != 0 || pvd[6] != 1)
        return -3;

    fs->portno = portno;
    fs->partition_lba = partition_lba;
    fs->logical_block_size = (uint16_t)(pvd[128] | (pvd[129] << 8));
    if (fs->logical_block_size == 0)
        fs->logical_block_size = ISO9660_SECTOR_SIZE;

    iso9660_dir_record_t* root = (iso9660_dir_record_t*)&pvd[156];
    fs->root_extent_lba = root->extent_lba_le;
    fs->root_size = root->data_len_le;

    return 0;
}

int iso9660_find_path(iso9660_fs_t* fs, const char* path, iso9660_dirent_t* out)
{
    if (!fs || !path || !out)
        return -1;

    if (path[0] == '\0') {
        out->extent_lba = fs->root_extent_lba;
        out->size = fs->root_size;
        out->flags = ISO9660_FLAG_DIR;
        return 0;
    }

    iso9660_dirent_t cur;
    cur.extent_lba = fs->root_extent_lba;
    cur.size = fs->root_size;
    cur.flags = ISO9660_FLAG_DIR;

    char tmp[256];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char* p = tmp;
    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;

        char* start = p;
        while (*p && *p != '/')
            p++;

        char saved = *p;
        *p = '\0';

        if (!(cur.flags & ISO9660_FLAG_DIR))
            return -2;

        if (iso_find_in_dir(fs, cur.extent_lba, cur.size, start, &cur) != 0)
            return -3;

        *p = saved;
    }

    *out = cur;
    return 0;
}

int iso9660_open(iso9660_fs_t* fs, const char* path, iso9660_file_t* out)
{
    if (!fs || !path || !out)
        return -1;

    iso9660_dirent_t e;
    if (iso9660_find_path(fs, path, &e) != 0)
        return -2;

    if (e.flags & ISO9660_FLAG_DIR)
        return -3;

    out->fs = fs;
    out->entry = e;
    out->pos = 0;
    return 0;
}

int iso9660_read(iso9660_file_t* f, uint8_t* out, uint32_t size)
{
    if (!f || !out)
        return -1;

    if (f->pos >= f->entry.size)
        return 0;

    uint32_t remaining = f->entry.size - f->pos;
    if (size > remaining)
        size = remaining;

    uint8_t* blk = kmalloc(f->fs->logical_block_size);
    if (!blk)
        return -2;

    uint32_t done = 0;

    while (done < size) {
        uint32_t abs = f->pos + done;
        uint32_t block_index = abs / f->fs->logical_block_size;
        uint32_t in_block = abs % f->fs->logical_block_size;

        if (iso_read_block(f->fs, f->entry.extent_lba + block_index, blk) != 0) {
            kfree(blk);
            return -3;
        }

        uint32_t take = f->fs->logical_block_size - in_block;
        if (take > (size - done))
            take = size - done;

        memcpy(out + done, blk + in_block, take);
        done += take;
    }

    f->pos += done;
    kfree(blk);
    return (int)done;
}

void iso9660_close(iso9660_file_t* f)
{
    if (f)
        memset(f, 0, sizeof(*f));
}

int iso9660_list_dir(iso9660_fs_t* fs, const iso9660_dirent_t* dir)
{
    if (!fs || !dir)
        return -1;

    uint8_t* blk = kmalloc(fs->logical_block_size);
    if (!blk)
        return -2;

    uint32_t blocks = (dir->size + fs->logical_block_size - 1) / fs->logical_block_size;

    for (uint32_t b = 0; b < blocks; b++) {
        if (iso_read_block(fs, dir->extent_lba + b, blk) != 0) {
            kfree(blk);
            return -3;
        }

        uint32_t off = 0;
        while (off < fs->logical_block_size) {
            iso9660_dir_record_t* r = (iso9660_dir_record_t*)(blk + off);
            if (r->length == 0)
                break;

            if (r->name_len == 1 && (uint8_t)r->name[0] <= 1) {
                off += r->length;
                continue;
            }

            char name[128];
            iso_name_to_vfs(r->name, r->name_len, name, sizeof(name));

            if (r->flags & ISO9660_FLAG_DIR)
                printfnoln(yellow_color "%s " reset_color, name);
            else
                printfnoln("%s ", name);

            off += r->length;
        }
    }

    kfree(blk);
    return 0;
}

int iso9660_list_root(iso9660_fs_t* fs)
{
    if (!fs)
        return -1;

    iso9660_dirent_t root = {
        .extent_lba = fs->root_extent_lba,
        .size = fs->root_size,
        .flags = ISO9660_FLAG_DIR,
    };

    return iso9660_list_dir(fs, &root);
}
