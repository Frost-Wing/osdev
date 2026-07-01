/**
 * @file ext2.c
 * @author Pradosh
 * @brief Production EXT2 read/write driver for FrostWing.
 * @version 0.1
 * @date 2025-12-31
 *
 * @copyright Copyright (c) Pradosh 2025
 *
 * Implements the on-disk EXT2 format as specified by Linux's ext2,
 * including direct/indirect/double-indirect/triple-indirect block
 * mapping, block & inode bitmap allocation, directory entry add/remove,
 * and standard open/read/write/create/mkdir/unlink operations.
 *
 * Block size is assumed <= EXT2_MAX_BLOCK_SIZE (4096 bytes), which
 * covers the overwhelming majority of real-world ext2 filesystems and
 * keeps all I/O on fixed-size stack buffers (no heap dependency, same
 * convention as the FAT16 driver in this tree).
 */

#include <filesystems/ext2.h>
#include <graphics.h>
#include <strings.h>
#include <memory.h>

#define EXT2_MAX_BLOCK_SIZE 4096U
#define EXT2_PTRS_PER_BLOCK_MAX (EXT2_MAX_BLOCK_SIZE / 4)

/* ===================== Low level block I/O ===================== */

static inline uint32_t ext2_block_to_lba(ext2_fs_t* fs, uint32_t block) {
    return fs->partition_lba + block * fs->sectors_per_block;
}

static int ext2_read_block(ext2_fs_t* fs, uint32_t block, void* buf) {
    if (block == 0) {
        memset(buf, 0, fs->block_size);
        return EXT2_OK;
    }
    if (ahci_read_sector(fs->portno, ext2_block_to_lba(fs, block), buf, fs->sectors_per_block) != 0)
        return EXT2_ERR_IO;
    return EXT2_OK;
}

static int ext2_write_block(ext2_fs_t* fs, uint32_t block, const void* buf) {
    if (block == 0)
        return EXT2_ERR_INVAL;
    if (ahci_write_sector(fs->portno, ext2_block_to_lba(fs, block), (void*)buf, fs->sectors_per_block) != 0)
        return EXT2_ERR_IO;
    return EXT2_OK;
}

static int ext2_zero_block(ext2_fs_t* fs, uint32_t block) {
    uint8_t zero[EXT2_MAX_BLOCK_SIZE];
    memset(zero, 0, fs->block_size);
    return ext2_write_block(fs, block, zero);
}

/* ===================== Superblock / group descriptors ===================== */

static int ext2_write_superblock(ext2_fs_t* fs) {
    uint8_t buf[1024];
    uint32_t sb_sector = fs->partition_lba + (EXT2_SUPERBLOCK_OFFSET / 512);

    if (ahci_read_sector(fs->portno, sb_sector, buf, 2) != 0)
        return EXT2_ERR_IO;

    memcpy(buf, &fs->sb, sizeof(ext2_superblock_t));

    if (ahci_write_sector(fs->portno, sb_sector, buf, 2) != 0)
        return EXT2_ERR_IO;

    fs->sb_dirty = 0;
    return EXT2_OK;
}

static int ext2_read_group_desc(ext2_fs_t* fs, uint32_t group, ext2_group_desc_t* out) {
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint32_t entries_per_block = fs->block_size / sizeof(ext2_group_desc_t);
    uint32_t blk = fs->gdt_block + (group / entries_per_block);
    uint32_t idx = group % entries_per_block;

    if (ext2_read_block(fs, blk, buf) != EXT2_OK)
        return EXT2_ERR_IO;

    memcpy(out, buf + idx * sizeof(ext2_group_desc_t), sizeof(ext2_group_desc_t));
    return EXT2_OK;
}

static int ext2_write_group_desc(ext2_fs_t* fs, uint32_t group, ext2_group_desc_t* gd) {
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint32_t entries_per_block = fs->block_size / sizeof(ext2_group_desc_t);
    uint32_t blk = fs->gdt_block + (group / entries_per_block);
    uint32_t idx = group % entries_per_block;

    if (ext2_read_block(fs, blk, buf) != EXT2_OK)
        return EXT2_ERR_IO;

    memcpy(buf + idx * sizeof(ext2_group_desc_t), gd, sizeof(ext2_group_desc_t));

    if (ext2_write_block(fs, blk, buf) != EXT2_OK)
        return EXT2_ERR_IO;

    return EXT2_OK;
}

partition_fs_type_t detect_ext2_type_enum(int portno, uint32_t partition_lba) {
    uint8_t buf[1024];
    uint32_t sb_sector = partition_lba + (EXT2_SUPERBLOCK_OFFSET / 512);

    if (ahci_read_sector(portno, sb_sector, buf, 2) != 0)
        return FS_UNKNOWN;

    ext2_superblock_t* sb = (ext2_superblock_t*)buf;
    if (sb->s_magic == EXT2_SUPER_MAGIC)
        return FS_EXT2;

    return FS_UNKNOWN;
}

int ext2_mount(int portno, uint32_t partition_lba, ext2_fs_t* fs) {
    uint8_t buf[1024];
    uint32_t sb_sector = partition_lba + (EXT2_SUPERBLOCK_OFFSET / 512);

    memset(fs, 0, sizeof(ext2_fs_t));

    if (ahci_read_sector(portno, sb_sector, buf, 2) != 0)
        return EXT2_ERR_IO;

    memcpy(&fs->sb, buf, sizeof(ext2_superblock_t));

    if (fs->sb.s_magic != EXT2_SUPER_MAGIC) {
        eprintf("ext2: bad magic 0x%x", fs->sb.s_magic);
        return EXT2_ERR_CORRUPT;
    }

    fs->portno = portno;
    fs->partition_lba = partition_lba;

    fs->block_size = 1024U << fs->sb.s_log_block_size;
    if (fs->block_size > EXT2_MAX_BLOCK_SIZE) {
        eprintf("ext2: block size %u unsupported (max %u)", fs->block_size, EXT2_MAX_BLOCK_SIZE);
        return EXT2_ERR_CORRUPT;
    }
    fs->sectors_per_block = fs->block_size / 512;

    fs->blocks_per_group = fs->sb.s_blocks_per_group;
    fs->inodes_per_group = fs->sb.s_inodes_per_group;

    if (fs->sb.s_rev_level >= EXT2_DYNAMIC_REV) {
        fs->inode_size = fs->sb.s_inode_size;
        if (fs->inode_size == 0) fs->inode_size = EXT2_GOOD_OLD_INODE_SIZE;
        fs->has_filetype = (fs->sb.s_feature_incompat & EXT2_FEATURE_INCOMPAT_FILETYPE) != 0;
    } else {
        fs->inode_size = EXT2_GOOD_OLD_INODE_SIZE;
        fs->has_filetype = 0;
    }

    fs->groups_count = (fs->sb.s_blocks_count - fs->sb.s_first_data_block +
                         fs->blocks_per_group - 1) / fs->blocks_per_group;

    /* GDT starts immediately after the block containing the superblock */
    fs->gdt_block = fs->sb.s_first_data_block + 1;

    fs->cwd_ino = EXT2_ROOT_INO;
    strcpy(fs->cwd_path, "/");
    fs->sb_dirty = 0;

    return EXT2_OK;
}

void ext2_unmount(ext2_fs_t* fs) {
    if (!fs) return;
    if (fs->sb_dirty)
        ext2_write_superblock(fs);
    memset(fs, 0, sizeof(ext2_fs_t));
}

/* ===================== Inode I/O ===================== */

static int ext2_inode_location(ext2_fs_t* fs, uint32_t ino, uint32_t* out_block, uint32_t* out_offset) {
    if (ino == 0) return EXT2_ERR_INVAL;

    uint32_t group = (ino - 1) / fs->inodes_per_group;
    uint32_t index = (ino - 1) % fs->inodes_per_group;

    ext2_group_desc_t gd;
    if (ext2_read_group_desc(fs, group, &gd) != EXT2_OK)
        return EXT2_ERR_IO;

    uint32_t inodes_per_block = fs->block_size / fs->inode_size;
    uint32_t block = gd.bg_inode_table + (index / inodes_per_block);
    uint32_t offset = (index % inodes_per_block) * fs->inode_size;

    *out_block = block;
    *out_offset = offset;
    return EXT2_OK;
}

static int ext2_read_inode(ext2_fs_t* fs, uint32_t ino, ext2_inode_t* out) {
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint32_t block, offset;

    if (ext2_inode_location(fs, ino, &block, &offset) != EXT2_OK)
        return EXT2_ERR_IO;

    if (ext2_read_block(fs, block, buf) != EXT2_OK)
        return EXT2_ERR_IO;

    memset(out, 0, sizeof(ext2_inode_t));
    memcpy(out, buf + offset, sizeof(ext2_inode_t) <= fs->inode_size ? sizeof(ext2_inode_t) : fs->inode_size);
    return EXT2_OK;
}

static int ext2_write_inode(ext2_fs_t* fs, uint32_t ino, ext2_inode_t* in) {
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint32_t block, offset;

    if (ext2_inode_location(fs, ino, &block, &offset) != EXT2_OK)
        return EXT2_ERR_IO;

    if (ext2_read_block(fs, block, buf) != EXT2_OK)
        return EXT2_ERR_IO;

    memcpy(buf + offset, in, sizeof(ext2_inode_t) <= fs->inode_size ? sizeof(ext2_inode_t) : fs->inode_size);

    if (ext2_write_block(fs, block, buf) != EXT2_OK)
        return EXT2_ERR_IO;

    return EXT2_OK;
}

/* ===================== Bitmap allocation ===================== */

static inline int bit_test(uint8_t* map, uint32_t bit) {
    return (map[bit / 8] >> (bit % 8)) & 1;
}
static inline void bit_set(uint8_t* map, uint32_t bit) {
    map[bit / 8] |= (uint8_t)(1u << (bit % 8));
}
static inline void bit_clear(uint8_t* map, uint32_t bit) {
    map[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

/* Allocate a free block, preferring the given group. Returns block number (>=1) or 0 on failure. */
static uint32_t ext2_alloc_block(ext2_fs_t* fs, uint32_t pref_group) {
    uint8_t bitmap[EXT2_MAX_BLOCK_SIZE];

    for (uint32_t gi = 0; gi < fs->groups_count; gi++) {
        uint32_t group = (pref_group + gi) % fs->groups_count;

        ext2_group_desc_t gd;
        if (ext2_read_group_desc(fs, group, &gd) != EXT2_OK)
            continue;
        if (gd.bg_free_blocks_count == 0)
            continue;

        if (ext2_read_block(fs, gd.bg_block_bitmap, bitmap) != EXT2_OK)
            continue;

        uint32_t blocks_in_group = fs->blocks_per_group;
        uint32_t remaining = fs->sb.s_blocks_count - fs->sb.s_first_data_block - group * fs->blocks_per_group;
        if (remaining < blocks_in_group) blocks_in_group = remaining;

        for (uint32_t b = 0; b < blocks_in_group; b++) {
            if (!bit_test(bitmap, b)) {
                bit_set(bitmap, b);
                if (ext2_write_block(fs, gd.bg_block_bitmap, bitmap) != EXT2_OK)
                    return 0;

                gd.bg_free_blocks_count--;
                ext2_write_group_desc(fs, group, &gd);

                fs->sb.s_free_blocks_count--;
                fs->sb_dirty = 1;
                ext2_write_superblock(fs);

                uint32_t block_no = fs->sb.s_first_data_block + group * fs->blocks_per_group + b;
                ext2_zero_block(fs, block_no);
                return block_no;
            }
        }
    }

    eprintf("ext2: no free blocks");
    return 0;
}

static void ext2_free_block(ext2_fs_t* fs, uint32_t block) {
    if (block == 0) return;

    uint32_t group = (block - fs->sb.s_first_data_block) / fs->blocks_per_group;
    uint32_t bit = (block - fs->sb.s_first_data_block) % fs->blocks_per_group;

    ext2_group_desc_t gd;
    if (ext2_read_group_desc(fs, group, &gd) != EXT2_OK)
        return;

    uint8_t bitmap[EXT2_MAX_BLOCK_SIZE];
    if (ext2_read_block(fs, gd.bg_block_bitmap, bitmap) != EXT2_OK)
        return;

    if (!bit_test(bitmap, bit))
        return; /* already free, avoid double counting */

    bit_clear(bitmap, bit);
    ext2_write_block(fs, gd.bg_block_bitmap, bitmap);

    gd.bg_free_blocks_count++;
    ext2_write_group_desc(fs, group, &gd);

    fs->sb.s_free_blocks_count++;
    fs->sb_dirty = 1;
    ext2_write_superblock(fs);
}

static uint32_t ext2_alloc_inode(ext2_fs_t* fs, uint32_t pref_group, int is_dir) {
    uint8_t bitmap[EXT2_MAX_BLOCK_SIZE];

    for (uint32_t gi = 0; gi < fs->groups_count; gi++) {
        uint32_t group = (pref_group + gi) % fs->groups_count;

        ext2_group_desc_t gd;
        if (ext2_read_group_desc(fs, group, &gd) != EXT2_OK)
            continue;
        if (gd.bg_free_inodes_count == 0)
            continue;

        if (ext2_read_block(fs, gd.bg_inode_bitmap, bitmap) != EXT2_OK)
            continue;

        for (uint32_t b = 0; b < fs->inodes_per_group; b++) {
            uint32_t ino = group * fs->inodes_per_group + b + 1;
            if (ino < fs->sb.s_first_ino && fs->sb.s_rev_level >= EXT2_DYNAMIC_REV && ino != EXT2_ROOT_INO)
                continue;

            if (!bit_test(bitmap, b)) {
                bit_set(bitmap, b);
                if (ext2_write_block(fs, gd.bg_inode_bitmap, bitmap) != EXT2_OK)
                    return 0;

                gd.bg_free_inodes_count--;
                if (is_dir) gd.bg_used_dirs_count++;
                ext2_write_group_desc(fs, group, &gd);

                fs->sb.s_free_inodes_count--;
                fs->sb_dirty = 1;
                ext2_write_superblock(fs);

                return ino;
            }
        }
    }

    eprintf("ext2: no free inodes");
    return 0;
}

static void ext2_free_inode(ext2_fs_t* fs, uint32_t ino, int is_dir) {
    if (ino == 0) return;

    uint32_t group = (ino - 1) / fs->inodes_per_group;
    uint32_t bit = (ino - 1) % fs->inodes_per_group;

    ext2_group_desc_t gd;
    if (ext2_read_group_desc(fs, group, &gd) != EXT2_OK)
        return;

    uint8_t bitmap[EXT2_MAX_BLOCK_SIZE];
    if (ext2_read_block(fs, gd.bg_inode_bitmap, bitmap) != EXT2_OK)
        return;

    if (!bit_test(bitmap, bit))
        return;

    bit_clear(bitmap, bit);
    ext2_write_block(fs, gd.bg_inode_bitmap, bitmap);

    gd.bg_free_inodes_count++;
    if (is_dir && gd.bg_used_dirs_count > 0) gd.bg_used_dirs_count--;
    ext2_write_group_desc(fs, group, &gd);

    fs->sb.s_free_inodes_count++;
    fs->sb_dirty = 1;
    ext2_write_superblock(fs);
}

/* ===================== Block mapping (direct/indirect) ===================== */

/*
 * Resolve logical block index -> physical block number for an inode.
 * If `alloc` is non-zero, missing blocks (including indirect blocks
 * themselves) are allocated and the inode's metadata is rewritten by
 * the caller after this returns (we write indirect blocks immediately
 * since they don't live in the inode struct).
 */
static uint32_t ext2_bmap(ext2_fs_t* fs, ext2_inode_t* inode, uint32_t lblock, int alloc, uint32_t pref_group) {
    uint32_t ptrs = fs->block_size / 4;

    if (lblock < EXT2_NDIR_BLOCKS) {
        if (inode->i_block[lblock] == 0 && alloc) {
            uint32_t nb = ext2_alloc_block(fs, pref_group);
            if (nb == 0) return 0;
            inode->i_block[lblock] = nb;
        }
        return inode->i_block[lblock];
    }
    lblock -= EXT2_NDIR_BLOCKS;

    if (lblock < ptrs) {
        uint32_t ind = inode->i_block[EXT2_IND_BLOCK];
        if (ind == 0) {
            if (!alloc) return 0;
            ind = ext2_alloc_block(fs, pref_group);
            if (ind == 0) return 0;
            inode->i_block[EXT2_IND_BLOCK] = ind;
        }

        uint32_t tbl[EXT2_PTRS_PER_BLOCK_MAX];
        if (ext2_read_block(fs, ind, tbl) != EXT2_OK) return 0;

        uint32_t result = tbl[lblock];
        if (result == 0 && alloc) {
            result = ext2_alloc_block(fs, pref_group);
            if (result == 0) return 0;
            tbl[lblock] = result;
            ext2_write_block(fs, ind, tbl);
        }
        return result;
    }
    lblock -= ptrs;

    if (lblock < ptrs * ptrs) {
        uint32_t dind = inode->i_block[EXT2_DIND_BLOCK];
        if (dind == 0) {
            if (!alloc) return 0;
            dind = ext2_alloc_block(fs, pref_group);
            if (dind == 0) return 0;
            inode->i_block[EXT2_DIND_BLOCK] = dind;
        }

        uint32_t l1[EXT2_PTRS_PER_BLOCK_MAX];
        if (ext2_read_block(fs, dind, l1) != EXT2_OK) return 0;

        uint32_t idx1 = lblock / ptrs;
        uint32_t idx2 = lblock % ptrs;

        uint32_t ind = l1[idx1];
        int wrote_l1 = 0;
        if (ind == 0) {
            if (!alloc) return 0;
            ind = ext2_alloc_block(fs, pref_group);
            if (ind == 0) return 0;
            l1[idx1] = ind;
            wrote_l1 = 1;
        }
        if (wrote_l1) ext2_write_block(fs, dind, l1);

        uint32_t l2[EXT2_PTRS_PER_BLOCK_MAX];
        if (ext2_read_block(fs, ind, l2) != EXT2_OK) return 0;

        uint32_t result = l2[idx2];
        if (result == 0 && alloc) {
            result = ext2_alloc_block(fs, pref_group);
            if (result == 0) return 0;
            l2[idx2] = result;
            ext2_write_block(fs, ind, l2);
        }
        return result;
    }
    lblock -= ptrs * ptrs;

    if (lblock < (uint32_t)ptrs * ptrs * ptrs) {
        uint32_t tind = inode->i_block[EXT2_TIND_BLOCK];
        if (tind == 0) {
            if (!alloc) return 0;
            tind = ext2_alloc_block(fs, pref_group);
            if (tind == 0) return 0;
            inode->i_block[EXT2_TIND_BLOCK] = tind;
        }

        uint32_t l1[EXT2_PTRS_PER_BLOCK_MAX];
        if (ext2_read_block(fs, tind, l1) != EXT2_OK) return 0;

        uint32_t idx1 = lblock / (ptrs * ptrs);
        uint32_t rem  = lblock % (ptrs * ptrs);
        uint32_t idx2 = rem / ptrs;
        uint32_t idx3 = rem % ptrs;

        uint32_t dind = l1[idx1];
        if (dind == 0) {
            if (!alloc) return 0;
            dind = ext2_alloc_block(fs, pref_group);
            if (dind == 0) return 0;
            l1[idx1] = dind;
            ext2_write_block(fs, tind, l1);
        }

        uint32_t l2[EXT2_PTRS_PER_BLOCK_MAX];
        if (ext2_read_block(fs, dind, l2) != EXT2_OK) return 0;

        uint32_t ind = l2[idx2];
        if (ind == 0) {
            if (!alloc) return 0;
            ind = ext2_alloc_block(fs, pref_group);
            if (ind == 0) return 0;
            l2[idx2] = ind;
            ext2_write_block(fs, dind, l2);
        }

        uint32_t l3[EXT2_PTRS_PER_BLOCK_MAX];
        if (ext2_read_block(fs, ind, l3) != EXT2_OK) return 0;

        uint32_t result = l3[idx3];
        if (result == 0 && alloc) {
            result = ext2_alloc_block(fs, pref_group);
            if (result == 0) return 0;
            l3[idx3] = result;
            ext2_write_block(fs, ind, l3);
        }
        return result;
    }

    return 0; /* file too large */
}

/* Recursively free all blocks referenced by an indirect block chain. */
static void ext2_free_indirect(ext2_fs_t* fs, uint32_t block, int depth) {
    if (block == 0) return;

    if (depth > 0) {
        uint32_t tbl[EXT2_PTRS_PER_BLOCK_MAX];
        if (ext2_read_block(fs, block, tbl) == EXT2_OK) {
            uint32_t ptrs = fs->block_size / 4;
            for (uint32_t i = 0; i < ptrs; i++)
                ext2_free_indirect(fs, tbl[i], depth - 1);
        }
    }
    ext2_free_block(fs, block);
}

static void ext2_truncate_inode(ext2_fs_t* fs, ext2_inode_t* inode) {
    for (int i = 0; i < EXT2_NDIR_BLOCKS; i++) {
        ext2_free_block(fs, inode->i_block[i]);
        inode->i_block[i] = 0;
    }
    ext2_free_indirect(fs, inode->i_block[EXT2_IND_BLOCK], 0);
    ext2_free_indirect(fs, inode->i_block[EXT2_DIND_BLOCK], 1);
    ext2_free_indirect(fs, inode->i_block[EXT2_TIND_BLOCK], 2);
    inode->i_block[EXT2_IND_BLOCK] = 0;
    inode->i_block[EXT2_DIND_BLOCK] = 0;
    inode->i_block[EXT2_TIND_BLOCK] = 0;
    inode->i_size = 0;
    inode->i_blocks = 0;
}

/* ===================== Directory entries ===================== */

typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    /* name follows, variable length */
} __attribute__((packed)) ext2_raw_dirent_t;

static inline uint16_t ext2_dirent_min_len(uint8_t name_len) {
    uint16_t len = EXT2_DIR_ENTRY_FIXED_SIZE + name_len;
    return (len + 3) & ~3; /* round to 4 */
}

/* Iterate directory entries of dir_ino; cb returns non-zero to stop. user gets raw entry + its absolute block + offset. */
typedef int (*ext2_dirent_cb)(ext2_fs_t* fs, uint32_t block, uint32_t off_in_block,
                               ext2_raw_dirent_t* de, const char* name, void* user);

static int ext2_iterate_dir(ext2_fs_t* fs, ext2_inode_t* dir, ext2_dirent_cb cb, void* user) {
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint32_t nblocks = (dir->i_size + fs->block_size - 1) / fs->block_size;

    for (uint32_t lb = 0; lb < nblocks; lb++) {
        uint32_t pb = ext2_bmap(fs, dir, lb, 0, 0);
        if (pb == 0) continue;
        if (ext2_read_block(fs, pb, buf) != EXT2_OK) continue;

        uint32_t off = 0;
        while (off < fs->block_size) {
            ext2_raw_dirent_t* de = (ext2_raw_dirent_t*)(buf + off);
            if (de->rec_len == 0) break;

            char name[EXT2_NAME_LEN + 1];
            if (de->inode != 0 && de->name_len > 0) {
                memcpy(name, buf + off + EXT2_DIR_ENTRY_FIXED_SIZE, de->name_len);
                name[de->name_len] = '\0';
            } else {
                name[0] = '\0';
            }

            if (cb(fs, pb, off, de, name, user))
                return 1;

            off += de->rec_len;
        }
    }
    return 0;
}

typedef struct {
    const char* target;
    uint32_t    found_ino;
    uint8_t     found_type;
    int         found;
} find_ctx_t;

static int find_cb(ext2_fs_t* fs, uint32_t block, uint32_t off, ext2_raw_dirent_t* de, const char* name, void* user) {
    (void)fs; (void)block; (void)off;
    find_ctx_t* ctx = user;
    if (de->inode != 0 && strcmp(name, ctx->target) == 0) {
        ctx->found_ino = de->inode;
        ctx->found_type = de->file_type;
        ctx->found = 1;
        return 1;
    }
    return 0;
}

int ext2_find_in_dir(ext2_fs_t* fs, uint32_t dir_ino, const char* name, uint32_t* out_ino, uint8_t* out_type) {
    ext2_inode_t dir;
    if (ext2_read_inode(fs, dir_ino, &dir) != EXT2_OK)
        return EXT2_ERR_IO;

    if ((dir.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
        return EXT2_ERR_NOTDIR;

    find_ctx_t ctx = { .target = name, .found = 0 };
    ext2_iterate_dir(fs, &dir, find_cb, &ctx);

    if (!ctx.found)
        return EXT2_ERR_NOT_FOUND;

    if (out_ino) *out_ino = ctx.found_ino;
    if (out_type) *out_type = ctx.found_type;
    return EXT2_OK;
}

typedef struct {
    char names[256][13]; /* unused placeholder to keep struct simple */
} unused_t;

static int list_cb(ext2_fs_t* fs, uint32_t block, uint32_t off, ext2_raw_dirent_t* de, const char* name, void* user) {
    (void)fs; (void)block; (void)off; (void)user;
    if (de->inode == 0) return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;

    if (de->file_type == EXT2_FT_DIR) {
        printfnoln(yellow_color "%s " reset_color, name);
    } else {
        printfnoln(blue_color "%s " reset_color, name);
    }
    return 0;
}

int ext2_list_dir(ext2_fs_t* fs, uint32_t dir_ino) {
    ext2_inode_t dir;
    if (ext2_read_inode(fs, dir_ino, &dir) != EXT2_OK)
        return EXT2_ERR_IO;

    if ((dir.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
        return EXT2_ERR_NOTDIR;

    ext2_iterate_dir(fs, &dir, list_cb, NULL);
    return EXT2_OK;
}

/* Insert a new directory entry (inode, name, type) into dir_ino, growing the
 * directory by one block if no existing entry has enough free space. */
static int ext2_dir_add_entry(ext2_fs_t* fs, uint32_t dir_ino, ext2_inode_t* dir,
                               uint32_t new_ino, const char* name, uint8_t type, uint32_t pref_group) {
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint8_t name_len = (uint8_t)strlen(name);
    uint16_t need = ext2_dirent_min_len(name_len);

    uint32_t nblocks = (dir->i_size + fs->block_size - 1) / fs->block_size;

    for (uint32_t lb = 0; lb < nblocks; lb++) {
        uint32_t pb = ext2_bmap(fs, dir, lb, 0, 0);
        if (pb == 0) continue;
        if (ext2_read_block(fs, pb, buf) != EXT2_OK) continue;

        uint32_t off = 0;
        while (off < fs->block_size) {
            ext2_raw_dirent_t* de = (ext2_raw_dirent_t*)(buf + off);
            if (de->rec_len == 0) break;

            uint16_t used = de->inode ? ext2_dirent_min_len(de->name_len) : 0;
            uint16_t avail = de->rec_len - used;

            if (avail >= need) {
                uint16_t old_rec_len = de->rec_len;

                if (de->inode != 0) {
                    /* split this entry */
                    de->rec_len = used;
                    ext2_raw_dirent_t* nde = (ext2_raw_dirent_t*)(buf + off + used);
                    nde->inode = new_ino;
                    nde->rec_len = old_rec_len - used;
                    nde->name_len = name_len;
                    nde->file_type = fs->has_filetype ? type : 0;
                    memcpy(buf + off + used + EXT2_DIR_ENTRY_FIXED_SIZE, name, name_len);
                } else {
                    de->inode = new_ino;
                    de->name_len = name_len;
                    de->file_type = fs->has_filetype ? type : 0;
                    de->rec_len = old_rec_len;
                    memcpy(buf + off + EXT2_DIR_ENTRY_FIXED_SIZE, name, name_len);
                }

                return ext2_write_block(fs, pb, buf);
            }

            off += de->rec_len;
        }
    }

    /* No space found: allocate a new block for the directory */
    uint32_t nb = ext2_alloc_block(fs, pref_group);
    if (nb == 0) return EXT2_ERR_NOSPACE;

    /* Hook it into the inode's block map at index nblocks */
    if (ext2_bmap(fs, dir, nblocks, 1, pref_group) == 0) {
        ext2_free_block(fs, nb);
        return EXT2_ERR_NOSPACE;
    }
    /* ext2_bmap with alloc=1 may have allocated a *different* block than nb
     * for direct entries (since it allocates internally); fetch what it set. */
    uint32_t real_block = ext2_bmap(fs, dir, nblocks, 0, 0);
    if (real_block == 0) return EXT2_ERR_NOSPACE;
    if (real_block != nb) {
        /* free our scratch alloc, we didn't need it for direct case */
        ext2_free_block(fs, nb);
        nb = real_block;
    }

    memset(buf, 0, fs->block_size);
    ext2_raw_dirent_t* nde = (ext2_raw_dirent_t*)buf;
    nde->inode = new_ino;
    nde->rec_len = (uint16_t)fs->block_size;
    nde->name_len = name_len;
    nde->file_type = fs->has_filetype ? type : 0;
    memcpy(buf + EXT2_DIR_ENTRY_FIXED_SIZE, name, name_len);

    if (ext2_write_block(fs, nb, buf) != EXT2_OK)
        return EXT2_ERR_IO;

    dir->i_size += fs->block_size;
    dir->i_blocks += fs->sectors_per_block;

    return ext2_write_inode(fs, dir_ino, dir);
}

/* Remove an entry by name; merges its space into the previous entry in the same block. */
static int ext2_dir_remove_entry(ext2_fs_t* fs, ext2_inode_t* dir, const char* name, uint32_t* removed_ino, uint8_t* removed_type) {
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint32_t nblocks = (dir->i_size + fs->block_size - 1) / fs->block_size;

    for (uint32_t lb = 0; lb < nblocks; lb++) {
        uint32_t pb = ext2_bmap(fs, dir, lb, 0, 0);
        if (pb == 0) continue;
        if (ext2_read_block(fs, pb, buf) != EXT2_OK) continue;

        uint32_t off = 0;
        uint32_t prev_off = (uint32_t)-1;

        while (off < fs->block_size) {
            ext2_raw_dirent_t* de = (ext2_raw_dirent_t*)(buf + off);
            if (de->rec_len == 0) break;

            if (de->inode != 0 && de->name_len == strlen(name) &&
                memcmp(buf + off + EXT2_DIR_ENTRY_FIXED_SIZE, name, de->name_len) == 0) {

                if (removed_ino) *removed_ino = de->inode;
                if (removed_type) *removed_type = de->file_type;

                if (prev_off != (uint32_t)-1) {
                    ext2_raw_dirent_t* prev = (ext2_raw_dirent_t*)(buf + prev_off);
                    prev->rec_len += de->rec_len;
                } else {
                    de->inode = 0;
                }

                return ext2_write_block(fs, pb, buf);
            }

            prev_off = off;
            off += de->rec_len;
        }
    }

    return EXT2_ERR_NOT_FOUND;
}

/* ===================== Path resolution ===================== */

static int ext2_split_first(const char* path, char* comp, size_t comp_sz, const char** rest) {
    while (*path == '/') path++;
    if (!*path) return -1;

    size_t i = 0;
    while (path[i] && path[i] != '/' && i < comp_sz - 1) {
        comp[i] = path[i];
        i++;
    }
    comp[i] = '\0';

    const char* r = path + i;
    while (*r == '/') r++;
    *rest = r;
    return 0;
}

int ext2_find_path(ext2_fs_t* fs, const char* path, uint32_t* out_ino, ext2_inode_t* out_inode) {
    if (!fs || !path) return EXT2_ERR_INVAL;

    uint32_t cur_ino = EXT2_ROOT_INO;

    if (!*path || strcmp(path, "/") == 0) {
        if (out_ino) *out_ino = cur_ino;
        if (out_inode) ext2_read_inode(fs, cur_ino, out_inode);
        return EXT2_OK;
    }

    char comp[EXT2_NAME_LEN + 1];
    const char* p = path;

    while (ext2_split_first(p, comp, sizeof(comp), &p) == 0) {
        uint32_t next_ino;
        uint8_t type;

        int rc = ext2_find_in_dir(fs, cur_ino, comp, &next_ino, &type);
        if (rc != EXT2_OK)
            return rc;

        cur_ino = next_ino;

        /* If there's more path left, this component must be a directory */
        if (*p) {
            ext2_inode_t tmp;
            if (ext2_read_inode(fs, cur_ino, &tmp) != EXT2_OK)
                return EXT2_ERR_IO;
            if ((tmp.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
                return EXT2_ERR_NOTDIR;
        }
    }

    if (out_ino) *out_ino = cur_ino;
    if (out_inode) ext2_read_inode(fs, cur_ino, out_inode);
    return EXT2_OK;
}

/* Resolve the parent directory of `path` and copy the final component into `name_out`. */
static int ext2_find_parent(ext2_fs_t* fs, const char* path, uint32_t* out_parent_ino, char* name_out, size_t name_sz) {
    const char* base = path + strlen(path);
    while (base > path && *(base - 1) == '/') base--;
    const char* end = base;
    while (base > path && *(base - 1) != '/') base--;

    if (base == path && *base != '/') {
        /* no slash at all -> parent is cwd-ish root */
        strncpy(name_out, path, name_sz - 1);
        name_out[name_sz - 1] = '\0';
        *out_parent_ino = EXT2_ROOT_INO;
        return EXT2_OK;
    }

    size_t namelen = (size_t)(end - base);
    if (namelen >= name_sz) namelen = name_sz - 1;
    memcpy(name_out, base, namelen);
    name_out[namelen] = '\0';

    char parent_path[256];
    size_t plen = (size_t)(base - path);
    if (plen == 0) plen = 1; /* root */
    if (plen >= sizeof(parent_path)) plen = sizeof(parent_path) - 1;
    memcpy(parent_path, path, plen);
    parent_path[plen] = '\0';
    if (plen > 1 && parent_path[plen - 1] == '/')
        parent_path[plen - 1] = '\0';
    if (parent_path[0] == '\0') strcpy(parent_path, "/");

    return ext2_find_path(fs, parent_path, out_parent_ino, NULL);
}

/* ===================== Time helper ===================== */

extern uint32_t get_unix_time(void); /* provided elsewhere in kernel; fallback below if absent */

static uint32_t ext2_now(void) {
#if defined(EXT2_HAVE_RTC)
    return get_unix_time();
#else
    return 0;
#endif
}

/* ===================== open / create / read / write / close ===================== */

int ext2_open(ext2_fs_t* fs, const char* path, ext2_file_t* f) {
    uint32_t ino;
    ext2_inode_t inode;

    int rc = ext2_find_path(fs, path, &ino, &inode);
    if (rc != EXT2_OK)
        return rc;

    memset(f, 0, sizeof(ext2_file_t));
    f->fs = fs;
    f->ino = ino;
    f->inode = inode;
    f->pos = 0;
    f->is_dir = (inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR;

    return EXT2_OK;
}

int ext2_create(ext2_fs_t* fs, const char* path, uint16_t mode, ext2_file_t* f) {
    uint32_t existing;
    if (ext2_find_path(fs, path, &existing, NULL) == EXT2_OK)
        return EXT2_ERR_EXISTS;

    char name[EXT2_NAME_LEN + 1];
    uint32_t parent_ino;
    if (ext2_find_parent(fs, path, &parent_ino, name, sizeof(name)) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (name[0] == '\0')
        return EXT2_ERR_INVAL;

    ext2_inode_t parent;
    if (ext2_read_inode(fs, parent_ino, &parent) != EXT2_OK)
        return EXT2_ERR_IO;
    if ((parent.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
        return EXT2_ERR_NOTDIR;

    uint32_t group = (parent_ino - 1) / fs->inodes_per_group;
    uint32_t new_ino = ext2_alloc_inode(fs, group, 0);
    if (new_ino == 0)
        return EXT2_ERR_NOSPACE;

    ext2_inode_t inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = EXT2_S_IFREG | (mode & 0x0FFF);
    inode.i_links_count = 1;
    inode.i_ctime = inode.i_atime = inode.i_mtime = ext2_now();
    inode.i_size = 0;
    inode.i_blocks = 0;

    if (ext2_write_inode(fs, new_ino, &inode) != EXT2_OK) {
        ext2_free_inode(fs, new_ino, 0);
        return EXT2_ERR_IO;
    }

    if (ext2_dir_add_entry(fs, parent_ino, &parent, new_ino, name, EXT2_FT_REG_FILE, group) != EXT2_OK) {
        ext2_free_inode(fs, new_ino, 0);
        return EXT2_ERR_NOSPACE;
    }

    if (f) {
        memset(f, 0, sizeof(ext2_file_t));
        f->fs = fs;
        f->ino = new_ino;
        f->inode = inode;
        f->pos = 0;
        f->is_dir = 0;
    }

    return EXT2_OK;
}

int ext2_read(ext2_file_t* f, uint8_t* out, uint32_t size) {
    if (!f || !f->fs) return EXT2_ERR_INVAL;
    ext2_fs_t* fs = f->fs;

    if (f->pos >= f->inode.i_size) return 0;
    if (f->pos + size > f->inode.i_size)
        size = f->inode.i_size - f->pos;
    if (size == 0) return 0;

    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint32_t total_read = 0;

    while (total_read < size) {
        uint32_t lblock = f->pos / fs->block_size;
        uint32_t off_in_block = f->pos % fs->block_size;
        uint32_t chunk = fs->block_size - off_in_block;
        if (chunk > size - total_read) chunk = size - total_read;

        uint32_t pb = ext2_bmap(fs, &f->inode, lblock, 0, 0);
        if (pb == 0) {
            memset(out + total_read, 0, chunk); /* sparse hole */
        } else {
            if (ext2_read_block(fs, pb, buf) != EXT2_OK)
                return total_read > 0 ? (int)total_read : EXT2_ERR_IO;
            memcpy(out + total_read, buf + off_in_block, chunk);
        }

        f->pos += chunk;
        total_read += chunk;
    }

    return (int)total_read;
}

int ext2_write(ext2_file_t* f, const uint8_t* data, uint32_t size) {
    if (!f || !f->fs) return EXT2_ERR_INVAL;
    if (f->is_dir) return EXT2_ERR_ISDIR;
    ext2_fs_t* fs = f->fs;

    if (size == 0) return 0;

    uint32_t group = (f->ino - 1) / fs->inodes_per_group;
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint32_t total_written = 0;

    while (total_written < size) {
        uint32_t lblock = f->pos / fs->block_size;
        uint32_t off_in_block = f->pos % fs->block_size;
        uint32_t chunk = fs->block_size - off_in_block;
        if (chunk > size - total_written) chunk = size - total_written;

        uint32_t pb = ext2_bmap(fs, &f->inode, lblock, 1, group);
        if (pb == 0)
            return total_written > 0 ? (int)total_written : EXT2_ERR_NOSPACE;

        if (off_in_block != 0 || chunk != fs->block_size) {
            if (ext2_read_block(fs, pb, buf) != EXT2_OK)
                return total_written > 0 ? (int)total_written : EXT2_ERR_IO;
        }

        memcpy(buf + off_in_block, data + total_written, chunk);

        if (ext2_write_block(fs, pb, buf) != EXT2_OK)
            return total_written > 0 ? (int)total_written : EXT2_ERR_IO;

        f->pos += chunk;
        total_written += chunk;

        if (f->pos > f->inode.i_size) {
            uint32_t old_blocks = (f->inode.i_size + fs->block_size - 1) / fs->block_size;
            uint32_t new_blocks = (f->pos + fs->block_size - 1) / fs->block_size;
            if (new_blocks > old_blocks)
                f->inode.i_blocks += (new_blocks - old_blocks) * fs->sectors_per_block;
            f->inode.i_size = f->pos;
        }
    }

    f->inode.i_mtime = ext2_now();
    if (ext2_write_inode(fs, f->ino, &f->inode) != EXT2_OK)
        return EXT2_ERR_IO;

    return (int)total_written;
}

void ext2_close(ext2_file_t* f) {
    if (!f || !f->fs) return;
    /* metadata is written eagerly on every ext2_write(), nothing to flush here */
    memset(f, 0, sizeof(ext2_file_t));
}

/* ===================== mkdir / unlink / rmdir ===================== */

int ext2_mkdir(ext2_fs_t* fs, const char* path) {
    uint32_t existing;
    if (ext2_find_path(fs, path, &existing, NULL) == EXT2_OK)
        return EXT2_ERR_EXISTS;

    char name[EXT2_NAME_LEN + 1];
    uint32_t parent_ino;
    if (ext2_find_parent(fs, path, &parent_ino, name, sizeof(name)) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (name[0] == '\0')
        return EXT2_ERR_INVAL;

    ext2_inode_t parent;
    if (ext2_read_inode(fs, parent_ino, &parent) != EXT2_OK)
        return EXT2_ERR_IO;
    if ((parent.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
        return EXT2_ERR_NOTDIR;

    uint32_t group = (parent_ino - 1) / fs->inodes_per_group;
    uint32_t new_ino = ext2_alloc_inode(fs, group, 1);
    if (new_ino == 0)
        return EXT2_ERR_NOSPACE;

    uint32_t first_block = ext2_alloc_block(fs, group);
    if (first_block == 0) {
        ext2_free_inode(fs, new_ino, 1);
        return EXT2_ERR_NOSPACE;
    }

    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    memset(buf, 0, fs->block_size);

    ext2_raw_dirent_t* dot = (ext2_raw_dirent_t*)buf;
    dot->inode = new_ino;
    dot->name_len = 1;
    dot->file_type = fs->has_filetype ? EXT2_FT_DIR : 0;
    dot->rec_len = ext2_dirent_min_len(1);
    buf[EXT2_DIR_ENTRY_FIXED_SIZE] = '.';

    ext2_raw_dirent_t* dotdot = (ext2_raw_dirent_t*)(buf + dot->rec_len);
    dotdot->inode = parent_ino;
    dotdot->name_len = 2;
    dotdot->file_type = fs->has_filetype ? EXT2_FT_DIR : 0;
    dotdot->rec_len = (uint16_t)(fs->block_size - dot->rec_len);
    buf[dot->rec_len + EXT2_DIR_ENTRY_FIXED_SIZE] = '.';
    buf[dot->rec_len + EXT2_DIR_ENTRY_FIXED_SIZE + 1] = '.';

    if (ext2_write_block(fs, first_block, buf) != EXT2_OK) {
        ext2_free_block(fs, first_block);
        ext2_free_inode(fs, new_ino, 1);
        return EXT2_ERR_IO;
    }

    ext2_inode_t inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = EXT2_S_IFDIR | 0755;
    inode.i_links_count = 2; /* '.' and parent's entry */
    inode.i_ctime = inode.i_atime = inode.i_mtime = ext2_now();
    inode.i_size = fs->block_size;
    inode.i_blocks = fs->sectors_per_block;
    inode.i_block[0] = first_block;

    if (ext2_write_inode(fs, new_ino, &inode) != EXT2_OK) {
        ext2_free_block(fs, first_block);
        ext2_free_inode(fs, new_ino, 1);
        return EXT2_ERR_IO;
    }

    if (ext2_dir_add_entry(fs, parent_ino, &parent, new_ino, name, EXT2_FT_DIR, group) != EXT2_OK) {
        ext2_free_block(fs, first_block);
        ext2_free_inode(fs, new_ino, 1);
        return EXT2_ERR_NOSPACE;
    }

    /* parent gained a subdirectory -> its own link count increases ('..' of child points to it) */
    parent.i_links_count++;
    ext2_write_inode(fs, parent_ino, &parent);

    return EXT2_OK;
}

int ext2_unlink_path(ext2_fs_t* fs, const char* path) {
    char name[EXT2_NAME_LEN + 1];
    uint32_t parent_ino;
    if (ext2_find_parent(fs, path, &parent_ino, name, sizeof(name)) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    ext2_inode_t parent;
    if (ext2_read_inode(fs, parent_ino, &parent) != EXT2_OK)
        return EXT2_ERR_IO;

    uint32_t target_ino;
    uint8_t target_type;
    if (ext2_find_in_dir(fs, parent_ino, name, &target_ino, &target_type) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (target_type == EXT2_FT_DIR)
        return EXT2_ERR_ISDIR;

    ext2_inode_t target;
    if (ext2_read_inode(fs, target_ino, &target) != EXT2_OK)
        return EXT2_ERR_IO;

    uint32_t removed_ino;
    if (ext2_dir_remove_entry(fs, &parent, name, &removed_ino, NULL) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (target.i_links_count > 0)
        target.i_links_count--;

    if (target.i_links_count == 0) {
        target.i_dtime = ext2_now();
        ext2_truncate_inode(fs, &target);
        ext2_write_inode(fs, target_ino, &target);
        ext2_free_inode(fs, target_ino, 0);
    } else {
        ext2_write_inode(fs, target_ino, &target);
    }

    return EXT2_OK;
}

int ext2_rmdir(ext2_fs_t* fs, const char* path) {
    char name[EXT2_NAME_LEN + 1];
    uint32_t parent_ino;
    if (ext2_find_parent(fs, path, &parent_ino, name, sizeof(name)) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    ext2_inode_t parent;
    if (ext2_read_inode(fs, parent_ino, &parent) != EXT2_OK)
        return EXT2_ERR_IO;

    uint32_t target_ino;
    uint8_t target_type;
    if (ext2_find_in_dir(fs, parent_ino, name, &target_ino, &target_type) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (target_type != EXT2_FT_DIR)
        return EXT2_ERR_NOTDIR;

    ext2_inode_t target;
    if (ext2_read_inode(fs, target_ino, &target) != EXT2_OK)
        return EXT2_ERR_IO;

    /* Directory must contain only '.' and '..' */
    int entry_count = 0;
    {
        uint8_t buf[EXT2_MAX_BLOCK_SIZE];
        uint32_t nblocks = (target.i_size + fs->block_size - 1) / fs->block_size;
        for (uint32_t lb = 0; lb < nblocks && entry_count <= 2; lb++) {
            uint32_t pb = ext2_bmap(fs, &target, lb, 0, 0);
            if (pb == 0) continue;
            if (ext2_read_block(fs, pb, buf) != EXT2_OK) continue;
            uint32_t off = 0;
            while (off < fs->block_size) {
                ext2_raw_dirent_t* de = (ext2_raw_dirent_t*)(buf + off);
                if (de->rec_len == 0) break;
                if (de->inode != 0) entry_count++;
                off += de->rec_len;
            }
        }
    }
    if (entry_count > 2)
        return EXT2_ERR_NOTEMPTY;

    if (ext2_dir_remove_entry(fs, &parent, name, NULL, NULL) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    ext2_truncate_inode(fs, &target);
    target.i_dtime = ext2_now();
    target.i_links_count = 0;
    ext2_write_inode(fs, target_ino, &target);
    ext2_free_inode(fs, target_ino, 1);

    if (parent.i_links_count > 0)
        parent.i_links_count--;
    ext2_write_inode(fs, parent_ino, &parent);

    return EXT2_OK;
}

/* ===================== truncate (public wrapper) ===================== */

int ext2_truncate(ext2_fs_t* fs, ext2_file_t* f) {
    if (!fs || !f) return EXT2_ERR_INVAL;
    if (f->is_dir) return EXT2_ERR_ISDIR;

    ext2_truncate_inode(fs, &f->inode);
    f->inode.i_mtime = ext2_now();
    f->pos = 0;

    return ext2_write_inode(fs, f->ino, &f->inode);
}

/* ===================== recursive remove ===================== */

static int ext2_rm_recursive_inode(ext2_fs_t* fs, uint32_t ino);

static int ext2_rm_recursive_cb(ext2_fs_t* fs, uint32_t block, uint32_t off,
                                 ext2_raw_dirent_t* de, const char* name, void* user) {
    (void)block; (void)off; (void)user;
    if (de->inode == 0) return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;

    ext2_rm_recursive_inode(fs, de->inode);
    return 0;
}

/* Frees an inode and, if it's a directory, recursively frees everything
 * beneath it first. Does NOT touch the parent's directory entry — the
 * caller is responsible for unlinking it from its parent before/after. */
static int ext2_rm_recursive_inode(ext2_fs_t* fs, uint32_t ino) {
    ext2_inode_t inode;
    if (ext2_read_inode(fs, ino, &inode) != EXT2_OK)
        return EXT2_ERR_IO;

    int is_dir = (inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR;

    if (is_dir) {
        /* recurse into every child first */
        ext2_iterate_dir(fs, &inode, ext2_rm_recursive_cb, NULL);
        inode.i_links_count = 0;
    } else {
        if (inode.i_links_count > 0)
            inode.i_links_count--;

        if (inode.i_links_count > 0) {
            /* still referenced elsewhere (hardlink) - just drop this link */
            ext2_write_inode(fs, ino, &inode);
            return EXT2_OK;
        }
    }

    ext2_truncate_inode(fs, &inode);
    inode.i_dtime = ext2_now();
    ext2_write_inode(fs, ino, &inode);
    ext2_free_inode(fs, ino, is_dir);

    return EXT2_OK;
}

int ext2_rm_recursive(ext2_fs_t* fs, const char* path) {
    if (!fs || !path) return EXT2_ERR_INVAL;

    char name[EXT2_NAME_LEN + 1];
    uint32_t parent_ino;
    if (ext2_find_parent(fs, path, &parent_ino, name, sizeof(name)) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return EXT2_ERR_INVAL;

    if (parent_ino == EXT2_ROOT_INO && strcmp(name, "") == 0)
        return EXT2_ERR_INVAL; /* refuse to rm "/" */

    ext2_inode_t parent;
    if (ext2_read_inode(fs, parent_ino, &parent) != EXT2_OK)
        return EXT2_ERR_IO;

    uint32_t target_ino;
    uint8_t target_type;
    if (ext2_find_in_dir(fs, parent_ino, name, &target_ino, &target_type) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (target_ino == EXT2_ROOT_INO)
        return EXT2_ERR_INVAL;

    if (ext2_dir_remove_entry(fs, &parent, name, NULL, NULL) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (target_type == EXT2_FT_DIR && parent.i_links_count > 0) {
        parent.i_links_count--;
        ext2_write_inode(fs, parent_ino, &parent);
    }

    return ext2_rm_recursive_inode(fs, target_ino);
}

/* ===================== rename / move ===================== */

int ext2_rename(ext2_fs_t* fs, const char* src_path, const char* dst_path) {
    if (!fs || !src_path || !dst_path)
        return EXT2_ERR_INVAL;

    /* destination must not already exist */
    if (ext2_find_path(fs, dst_path, NULL, NULL) == EXT2_OK)
        return EXT2_ERR_EXISTS;

    char src_name[EXT2_NAME_LEN + 1];
    uint32_t src_parent_ino;
    if (ext2_find_parent(fs, src_path, &src_parent_ino, src_name, sizeof(src_name)) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    char dst_name[EXT2_NAME_LEN + 1];
    uint32_t dst_parent_ino;
    if (ext2_find_parent(fs, dst_path, &dst_parent_ino, dst_name, sizeof(dst_name)) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (src_name[0] == '\0' || dst_name[0] == '\0')
        return EXT2_ERR_INVAL;

    uint32_t src_ino;
    uint8_t src_type;
    if (ext2_find_in_dir(fs, src_parent_ino, src_name, &src_ino, &src_type) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (src_ino == EXT2_ROOT_INO)
        return EXT2_ERR_INVAL;

    /* refuse to move a directory into its own subtree */
    if (src_type == EXT2_FT_DIR) {
        uint32_t walk = dst_parent_ino;
        while (1) {
            if (walk == src_ino)
                return EXT2_ERR_INVAL;
            if (walk == EXT2_ROOT_INO)
                break;

            uint32_t up;
            uint8_t up_type;
            ext2_inode_t walk_inode;
            if (ext2_read_inode(fs, walk, &walk_inode) != EXT2_OK)
                break;
            if (ext2_find_in_dir(fs, walk, "..", &up, &up_type) != EXT2_OK)
                break;
            if (up == walk)
                break;
            walk = up;
        }
    }

    ext2_inode_t src_parent, dst_parent;
    if (ext2_read_inode(fs, src_parent_ino, &src_parent) != EXT2_OK)
        return EXT2_ERR_IO;
    if (ext2_read_inode(fs, dst_parent_ino, &dst_parent) != EXT2_OK)
        return EXT2_ERR_IO;

    uint32_t group = (dst_parent_ino - 1) / fs->inodes_per_group;

    if (ext2_dir_add_entry(fs, dst_parent_ino, &dst_parent, src_ino, dst_name, src_type, group) != EXT2_OK)
        return EXT2_ERR_NOSPACE;

    if (ext2_dir_remove_entry(fs, &src_parent, src_name, NULL, NULL) != EXT2_OK)
        return EXT2_ERR_NOT_FOUND;

    if (src_type == EXT2_FT_DIR && src_parent_ino != dst_parent_ino) {
        /* fix up the moved directory's '..' entry to point at its new parent */
        ext2_inode_t moved;
        if (ext2_read_inode(fs, src_ino, &moved) == EXT2_OK) {
            uint8_t buf[EXT2_MAX_BLOCK_SIZE];
            uint32_t first_block = moved.i_block[0];

            if (first_block && ext2_read_block(fs, first_block, buf) == EXT2_OK) {
                ext2_raw_dirent_t* dot = (ext2_raw_dirent_t*)buf;
                if (dot->rec_len > 0 && dot->rec_len < fs->block_size) {
                    ext2_raw_dirent_t* dotdot = (ext2_raw_dirent_t*)(buf + dot->rec_len);
                    dotdot->inode = dst_parent_ino;
                    ext2_write_block(fs, first_block, buf);
                }
            }
        }

        /* the directory's '..' no longer points at src_parent, and now
         * points at dst_parent, so link counts follow it */
        if (src_parent.i_links_count > 0)
            src_parent.i_links_count--;
        dst_parent.i_links_count++;

        ext2_write_inode(fs, src_parent_ino, &src_parent);
        ext2_write_inode(fs, dst_parent_ino, &dst_parent);
    }

    return EXT2_OK;
}

int ext2_sync(ext2_fs_t *fs)
{
    if (!fs)
        return EXT2_ERR_INVAL;

    /*
     * If, for some reason, the superblock is still marked dirty,
     * commit it now.
     */
    if (fs->sb_dirty) {
        int rc = ext2_write_superblock(fs);
        if (rc != EXT2_OK)
            return rc;
    }

    /*
     * Future:
     *  - Flush inode cache
     *  - Flush block cache
     *  - Flush bitmap cache
     *  - Flush directory cache
     *  - ATA FLUSH CACHE
     */

    return EXT2_OK;
}