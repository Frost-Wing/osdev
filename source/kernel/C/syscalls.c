/**
 * @file syscalls.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-01-05
 *
 * @copyright Copyright (c) Pradosh 2024
 *
 */
#include <commands/login.h>
#include <stdint.h>
#include <syscalls.h>
#include <limine.h>
#include <keyboard.h>
#include <graphics.h>
#include <memory.h>
#include <stream.h>
#include <userland.h>

// Entire filesystems present in the OS.
#include <filesystems/vfs.h>
#include <filesystems/layers/proc.h>
#include <filesystems/layers/dev.h>
#include <filesystems/fat16.h>
#include <filesystems/fat32.h>
#include <filesystems/iso9660.h>

#include <executables/elf.h>
#include <ahci.h>
#include <rtc.h>
#include <heap.h>
#include <tty.h>

// sys headers
#include <sys/dirent.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/helper.h>

extern struct limine_framebuffer *framebuffer;
extern int64* font_address;

extern int execute_chain(const char* line);

typedef struct {
    bool exists;
    bool is_dir;
    uint64_t size;
    uint32_t mode;
    uint64_t inode;
} vfs_stat_info_t;

static char current_exec_path[256] = "/";
static uint64_t current_fs_base = 0;

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
} linux_iso9660_dir_record_t;
#pragma pack(pop)

static void iso_name_to_vfs_local(const char* in, uint8_t in_len, char* out, size_t out_sz) {
    size_t oi = 0;
    for (uint8_t i = 0; i < in_len && oi + 1 < out_sz; ++i) {
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

static int linux_flags_to_vfs(int linux_flags) {
    int vfs_flags = 0;
    int access = linux_flags & 0x3;

    switch (access) {
        case LINUX_O_WRONLY:
            vfs_flags |= VFS_WRONLY;
            break;
        case LINUX_O_RDWR:
            vfs_flags |= VFS_RDWR;
            break;
        case LINUX_O_RDONLY:
        default:
            vfs_flags |= VFS_RDONLY;
            break;
    }

    if (linux_flags & LINUX_O_CREAT)
        vfs_flags |= VFS_CREATE;
    if (linux_flags & LINUX_O_TRUNC)
        vfs_flags |= VFS_TRUNC;
    if (linux_flags & LINUX_O_APPEND)
        vfs_flags |= VFS_APPEND;

    return vfs_flags;
}

static void fill_stat_from_info(linux_stat_t* st, const vfs_stat_info_t* info) {
    if (!st || !info)
        return;

    memset(st, 0, sizeof(*st));
    st->st_dev = 1;
    st->st_ino = info->inode;
    st->st_nlink = info->is_dir ? 2 : 1;
    st->st_mode = info->mode;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = info->is_dir ? 0 : 1;
    st->st_size = (int64_t)info->size;
    st->st_blksize = 512;
    st->st_blocks = (info->size + 511) / 512;
}

static bool resolve_path_at(int dirfd, const char* path, char* out, size_t out_sz) {
    if (!path || !out)
        return false;

    if (path[0] == '/')
        return vfs_normalize_path(path, out, out_sz) == 0;

    if (dirfd == LINUX_AT_FDCWD || dirfd == 0)
        return vfs_normalize_path(path, out, out_sz) == 0;

    if (!fd_valid(dirfd))
        return false;

    const char* base = fd_get_path(dirfd);
    if (!base)
        base = vfs_getcwd();

    char joined[512];
    snprintf(joined, sizeof(joined), "%s/%s", base, path);
    return vfs_normalize_path(joined, out, out_sz) == 0;
}

static bool fill_vfs_stat_for_path_at(int dirfd, const char* path, vfs_stat_info_t* info) {
    if (!path || !info)
        return false;

    memset(info, 0, sizeof(*info));

    char norm[256];
    if (!resolve_path_at(dirfd, path, norm, sizeof(norm)))
        return false;

    if (strcmp(norm, "/") == 0) {
        info->exists = true;
        info->is_dir = true;
        info->size = 0;
        info->mode = LINUX_S_IFDIR | 0755;
        info->inode = 1;
        return true;
    }

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return false;

    info->exists = true;
    info->inode = path_inode_hash(norm);

    if (res.mnt->type == FS_PROC) {
        if (res.rel_path[0] == '\0') {
            info->is_dir = true;
            info->mode = LINUX_S_IFDIR | 0555;
            return true;
        }

        vfs_file_t procf = {0};
        snprintf(procf.rel_path, sizeof(procf.rel_path), "%s", res.rel_path);
        procf.mnt = res.mnt;
        if (procfs_open(&procf) != 0)
            return false;
        info->is_dir = false;
        info->mode = LINUX_S_IFREG | 0444;
        info->size = 0;
        return true;
    }

    if (res.mnt->type == FS_DEV) {
        if (res.rel_path[0] == '\0') {
            info->is_dir = true;
            info->mode = LINUX_S_IFDIR | 0555;
            return true;
        }

        vfs_file_t devf = {0};
        snprintf(devf.rel_path, sizeof(devf.rel_path), "%s", res.rel_path);
        devf.mnt = res.mnt;
        if (devfs_open(&devf) != 0)
            return false;
        info->is_dir = false;
        info->mode = LINUX_S_IFCHR | 0666;
        info->size = 0;
        return true;
    }

    if (res.rel_path[0] == '\0') {
        info->is_dir = true;
        info->mode = LINUX_S_IFDIR | 0755;
        return true;
    }

    if (res.mnt->type == FS_FAT16) {
        fat16_fs_t* fs = (fat16_fs_t*)res.mnt->fs;
        fat16_dir_entry_t entry = {0};
        if (fat16_find_path(fs, res.rel_path, &entry) != 0)
            return false;
        info->is_dir = (entry.attr & 0x10) != 0;
        info->size = entry.filesize;
    } else if (res.mnt->type == FS_FAT32) {
        fat32_fs_t* fs = (fat32_fs_t*)res.mnt->fs;
        fat32_dir_entry_t entry = {0};
        if (fat32_find_path(fs, res.rel_path, &entry) != FAT_OK)
            return false;
        info->is_dir = (entry.attr & FAT_ATTR_DIRECTORY) != 0;
        info->size = entry.file_size;
    } else if (res.mnt->type == FS_ISO9660) {
        iso9660_fs_t* fs = (iso9660_fs_t*)res.mnt->fs;
        iso9660_dirent_t entry = {0};
        if (iso9660_find_path(fs, res.rel_path, &entry) != 0)
            return false;
        info->is_dir = (entry.flags & ISO9660_FLAG_DIR) != 0;
        info->size = entry.size;
    } else {
        return false;
    }

    info->mode = (info->is_dir ? LINUX_S_IFDIR | 0755 : LINUX_S_IFREG | 0644);
    return true;
}

static bool fill_vfs_stat_for_fd(int fd, vfs_stat_info_t* info) {
    if (!info || !fd_valid(fd))
        return false;

    memset(info, 0, sizeof(*info));
    info->exists = true;
    info->inode = (uint64_t)(fd + 1);

    if (fd <= STDERR || fd_get_file(fd) == NULL) {
        info->is_dir = false;
        info->size = 0;
        info->mode = LINUX_S_IFCHR | 0666;
        info->inode = (uint64_t)(fd + 3);
        return true;
    }

    vfs_file_t* file = fd_get_file(fd);
    switch (file->mnt->type) {
        case FS_PROC:
        case FS_DEV:
            info->is_dir = false;
            info->size = 0;
            break;
        case FS_FAT16:
            info->is_dir = (file->f.fat16.entry.attr & 0x10) != 0;
            info->size = file->f.fat16.entry.filesize;
            break;
        case FS_FAT32:
            info->is_dir = (file->f.fat32.entry.attr & FAT_ATTR_DIRECTORY) != 0;
            info->size = file->f.fat32.entry.file_size;
            break;
        case FS_ISO9660:
            info->is_dir = (file->f.iso9660.entry.flags & ISO9660_FLAG_DIR) != 0;
            info->size = file->f.iso9660.entry.size;
            break;
        default:
            return false;
    }

    info->mode = info->is_dir ? (LINUX_S_IFDIR | 0755) : (LINUX_S_IFREG | 0644);
    return true;
}

static int64 copy_readlink_result(const char* target, char* buf, uint64_t bufsiz) {
    if (!target || !buf || bufsiz == 0)
        return -LINUX_EINVAL;

    uint64_t len = strlen(target);
    if (len > bufsiz)
        len = bufsiz;
    memcpy(buf, target, len);
    return (int64)len;
}

static int emit_dirent(char* buf, uint64_t buflen, uint64_t* used, uint64_t ino, uint8_t type, const char* name, uint64_t next_off) {
    uint64_t name_len = strlen(name) + 1;
    uint64_t reclen = sizeof(linux_dirent64_t) + name_len;
    reclen = (reclen + 7) & ~7ULL;

    if (*used + reclen > buflen)
        return 0;

    linux_dirent64_t* ent = (linux_dirent64_t*)(buf + *used);
    ent->d_ino = ino;
    ent->d_off = next_off;
    ent->d_reclen = (uint16_t)reclen;
    ent->d_type = type;
    memcpy(ent->d_name, name, name_len);
    memset(((char*)ent) + sizeof(linux_dirent64_t) + name_len, 0, reclen - sizeof(linux_dirent64_t) - name_len);
    *used += reclen;
    return 1;
}

static uint32_t fat16_cluster_lba_local(fat16_fs_t* fs, uint16_t cluster) {
    if (cluster == FAT16_ROOT_CLUSTER)
        return fs->root_dir_start;
    return fs->data_start + ((uint32_t)(cluster - 2) * fs->bs.sectors_per_cluster);
}

static void fat32_short_name(const fat32_dir_entry_t* e, char* out, size_t out_sz) {
    char name[9];
    char ext[4];

    memcpy(name, e->name, 8);
    memcpy(ext, e->name + 8, 3);
    name[8] = 0;
    ext[3] = 0;

    for (int i = 7; i >= 0 && name[i] == ' '; --i)
        name[i] = 0;
    for (int i = 2; i >= 0 && ext[i] == ' '; --i)
        ext[i] = 0;

    if (ext[0] != '\0')
        snprintf(out, out_sz, "%s.%s", name, ext);
    else
        snprintf(out, out_sz, "%s", name);
}

static int sys_getdents64(uint64_t fd, char* buf, uint64_t buflen) {
    if (!buf || buflen < sizeof(linux_dirent64_t))
        return -LINUX_EINVAL;
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    vfs_file_t* file = fd_get_file((int)fd);
    if (!file) {
        return -LINUX_ENOTDIR;
    }

    uint32_t* pos = fd_pos_ptr((int)fd);
    if (!pos)
        return -LINUX_ENOTDIR;

    uint64_t used = 0;
    uint64_t entry_index = *pos;

    if (file->mnt->type == FS_PROC) {
        static const char* proc_entries[PROC_FILE_COUNT] = {"stat", "heap", "meminfo"};
        for (uint64_t i = entry_index; i < PROC_FILE_COUNT; ++i) {
            if (!emit_dirent(buf, buflen, &used, i + 2, 8, proc_entries[i], i + 1))
                break;
            *pos = (uint32_t)(i + 1);
        }
        return (int)used;
    }

    if (file->mnt->type == FS_DEV) {
        static const char* dev_entries[] = {"null", "zero", "random", "urandom"};
        const uint64_t fixed = 4;
        uint64_t total = fixed;
        for (int i = 0; i < block_device_count; i++) {
            if (block_devices[i].present &&
                (block_devices[i].type == BLOCK_DEVICE_AHCI || block_devices[i].type == BLOCK_DEVICE_NVME))
                total++;
        }

        for (uint64_t i = entry_index; i < total; ++i) {
            const char* name = NULL;

            if (i < fixed) {
                name = dev_entries[i];
            } else {
                uint64_t disk_idx = i - fixed;
                uint64_t seen = 0;
                for (int j = 0; j < block_device_count; j++) {
                    if (!block_devices[j].present)
                        continue;
                    if (block_devices[j].type != BLOCK_DEVICE_AHCI && block_devices[j].type != BLOCK_DEVICE_NVME)
                        continue;

                    if (seen++ == disk_idx) {
                        name = block_devices[j].name;
                        break;
                    }
                }
            }

            if (!name)
                continue;

            if (!emit_dirent(buf, buflen, &used, path_inode_hash(name), 2, name, i + 1))
                break;

            *pos = (uint32_t)(i + 1);
        }
        return (int)used;
    }

    if (file->mnt->type == FS_FAT16) {
        fat16_fs_t* fs = file->f.fat16.fs;
        uint16_t cluster = file->f.fat16.entry.first_cluster;
        uint8_t sector[SECTOR_SIZE];
        uint64_t idx = 0;

        if ((file->f.fat16.entry.attr & 0x10) == 0 && file->f.fat16.entry.first_cluster == 0 && file->f.fat16.entry.filesize == 0)
            cluster = FAT16_ROOT_CLUSTER;

        if (cluster == FAT16_ROOT_CLUSTER) {
            for (uint32_t s = 0; s < fs->root_dir_sectors; ++s) {
                ahci_read_sector(fs->portno, fs->root_dir_start + s, sector, 1);
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector;
                for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i) {
                    fat16_dir_entry_t* e = &entries[i];
                    if (e->name[0] == 0x00)
                        return (int)used;
                    if (e->name[0] == 0xE5 || (e->attr & 0x0F) == 0x0F || (e->attr & 0x08))
                        continue;
                    if (idx++ < entry_index)
                        continue;
                    char name[16];
                    fat16_unformat_name(e, name);
                    if (!emit_dirent(buf, buflen, &used, path_inode_hash(name), (e->attr & 0x10) ? 4 : 8, name, idx))
                        return (int)used;
                    *pos = (uint32_t)idx;
                }
            }
            return (int)used;
        }

        while (cluster < FAT16_EOC) {
            uint32_t lba = fat16_cluster_lba_local(fs, cluster);
            for (uint32_t s = 0; s < fs->bs.sectors_per_cluster; ++s) {
                ahci_read_sector(fs->portno, lba + s, sector, 1);
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector;
                for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i) {
                    fat16_dir_entry_t* e = &entries[i];
                    if (e->name[0] == 0x00)
                        return (int)used;
                    if (e->name[0] == 0xE5 || (e->attr & 0x0F) == 0x0F || (e->attr & 0x08))
                        continue;
                    if (idx++ < entry_index)
                        continue;
                    char name[16];
                    fat16_unformat_name(e, name);
                    if (!emit_dirent(buf, buflen, &used, path_inode_hash(name), (e->attr & 0x10) ? 4 : 8, name, idx))
                        return (int)used;
                    *pos = (uint32_t)idx;
                }
            }
            cluster = fat16_read_fat_fs(fs, cluster);
        }
        return (int)used;
    }

    if (file->mnt->type == FS_FAT32) {
        fat32_fs_t* fs = file->f.fat32.fs;
        uint32_t cluster = file->f.fat32.is_dir ? file->f.fat32.start_cluster : fs->root_cluster;
        uint8_t sector[FAT32_SECTOR_SIZE];
        uint64_t idx = 0;

        if (!file->f.fat32.is_dir && file->f.fat32.entry.attr == 0 && file->f.fat32.entry.file_size == 0)
            cluster = fs->root_cluster;

        while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
            uint32_t cluster_lba = fs->data_start_lba + ((cluster - 2) * fs->sectors_per_cluster);
            for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
                ahci_read_sector(fs->portno, cluster_lba + s, sector, 1);
                for (uint32_t off = 0; off < FAT32_SECTOR_SIZE; off += sizeof(fat32_dir_entry_t)) {
                    fat32_dir_entry_t* e = (fat32_dir_entry_t*)(sector + off);
                    if (e->name[0] == 0x00)
                        return (int)used;
                    if (e->name[0] == 0xE5 || e->attr == FAT_ATTR_LFN || (e->attr & FAT_ATTR_VOLUME_ID))
                        continue;
                    if (idx++ < entry_index)
                        continue;
                    char name[20];
                    fat32_short_name(e, name, sizeof(name));
                    if (!emit_dirent(buf, buflen, &used, path_inode_hash(name), (e->attr & FAT_ATTR_DIRECTORY) ? 4 : 8, name, idx))
                        return (int)used;
                    *pos = (uint32_t)idx;
                }
            }
            uint32_t fat_sector = fs->fat_start_lba + (cluster * 4 / FAT32_SECTOR_SIZE);
            uint32_t ent_offset = (cluster * 4) % FAT32_SECTOR_SIZE;
            ahci_read_sector(fs->portno, fat_sector, sector, 1);
            cluster = (*(uint32_t*)(sector + ent_offset)) & 0x0FFFFFFF;
        }
        return (int)used;
    }

    if (file->mnt->type == FS_ISO9660) {
        iso9660_fs_t* fs = file->f.iso9660.fs;
        iso9660_dirent_t dir = file->f.iso9660.entry;
        if ((dir.flags & ISO9660_FLAG_DIR) == 0) {
            dir.extent_lba = fs->root_extent_lba;
            dir.size = fs->root_size;
            dir.flags = ISO9660_FLAG_DIR;
        }

        uint8_t* block = kmalloc(fs->logical_block_size);
        if (!block)
            return -LINUX_ENOMEM;

        uint64_t idx = 0;
        uint32_t blocks = (dir.size + fs->logical_block_size - 1) / fs->logical_block_size;
        for (uint32_t b = 0; b < blocks; ++b) {
            if (ahci_read_sector(fs->portno, dir.extent_lba + (b * (fs->logical_block_size / SECTOR_SIZE)), block, fs->logical_block_size / SECTOR_SIZE) != 0)
                break;
            uint32_t off = 0;
            while (off < fs->logical_block_size) {
                linux_iso9660_dir_record_t* r = (linux_iso9660_dir_record_t*)(block + off);
                if (r->length == 0)
                    break;
                if (r->name_len == 1 && (uint8_t)r->name[0] <= 1) {
                    off += r->length;
                    continue;
                }
                if (idx++ >= entry_index) {
                    char name[128];
                    iso_name_to_vfs_local(r->name, r->name_len, name, sizeof(name));
                    if (!emit_dirent(buf, buflen, &used, path_inode_hash(name), (r->flags & ISO9660_FLAG_DIR) ? 4 : 8, name, idx))
                        goto iso_done;
                    *pos = (uint32_t)idx;
                }
                off += r->length;
            }
        }
iso_done:
        kfree(block);
        return (int)used;
    }

    return -LINUX_ENOTDIR;
}

static int64 sys_open_common(int dirfd, const char* path, int flags, int mode) {
    (void)mode;

    if (path == NULL)
        return -LINUX_EINVAL;

    char resolved_path[256];
    if (!resolve_path_at(dirfd, path, resolved_path, sizeof(resolved_path)))
        return -LINUX_EINVAL;

    int fd = fd_open(resolved_path, linux_flags_to_vfs(flags));
    if (fd == -1)
        return -LINUX_ENFILE;

    if (fd == -2) {
        return -LINUX_ENOENT;
    }

    return fd;
}

static int64 sys_close(uint64_t fd) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    return fd_close((int)fd) == 0 ? 0 : -LINUX_EBADF;
}

static int64 sys_fstat(uint64_t fd, linux_stat_t* st) {
    vfs_stat_info_t info;
    if (!st)
        return -LINUX_EINVAL;
    if (!fill_vfs_stat_for_fd((int)fd, &info))
        return -LINUX_EBADF;
    fill_stat_from_info(st, &info);
    return 0;
}

static int64 sys_stat(const char* path, linux_stat_t* st) {
    if (!path || !st)
        return -LINUX_EINVAL;

    vfs_stat_info_t info;
    if (!fill_vfs_stat_for_path_at(LINUX_AT_FDCWD, path, &info))
        return -LINUX_ENOENT;

    fill_stat_from_info(st, &info);
    return 0;
}

static int64 sys_newfstatat(int dirfd, const char* path, linux_stat_t* st, int flags) {
    (void)flags;
    if (!st)
        return -LINUX_EINVAL;
    if ((flags & ~(LINUX_AT_SYMLINK_NOFOLLOW | LINUX_AT_EMPTY_PATH)) != 0)
        return -LINUX_EINVAL;

    if ((flags & LINUX_AT_EMPTY_PATH) && path && path[0] == '\0')
        return sys_fstat((uint64_t)dirfd, st);

    if (!path)
        return -LINUX_EINVAL;
    vfs_stat_info_t info;
    if (!fill_vfs_stat_for_path_at(dirfd, path, &info))
        return -LINUX_ENOENT;
    fill_stat_from_info(st, &info);
    return 0;
}

static int64 sys_statx(int dirfd, const char* path, int flags, unsigned int mask, linux_statx_t* stx) {
    (void)mask;
    linux_stat_t st;
    if (!stx)
        return -LINUX_EINVAL;
    int64 rc = sys_newfstatat(dirfd, path, &st, flags);
    if (rc < 0)
        return rc;

    memset(stx, 0, sizeof(*stx));
    stx->stx_blksize = (uint32_t)st.st_blksize;
    stx->stx_nlink = (uint32_t)st.st_nlink;
    stx->stx_uid = st.st_uid;
    stx->stx_gid = st.st_gid;
    stx->stx_mode = (uint16_t)st.st_mode;
    stx->stx_ino = st.st_ino;
    stx->stx_size = st.st_size;
    stx->stx_blocks = st.st_blocks;
    return 0;
}

static int64 sys_read(uint64_t fd, char* buf, uint64_t count) {
    if (buf == NULL || count == 0)
        return 0;

    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    int flags = fd_flags((int)fd);
    if (!(flags & VFS_RDONLY) && !(flags & VFS_RDWR))
        return -LINUX_EBADF;

    vfs_file_t* file = fd_get_file((int)fd);
    if (fd == 0)
        return tty_read(buf, count);

    int rd = vfs_read(file, (uint8_t*)buf, (uint32_t)count);
    if (rd < 0)
        return -LINUX_EBADF;

    return rd;
}

static int64 sys_write(uint64_t fd, const char* buf, uint64_t count) {
    if (buf == NULL || count == 0)
        return 0;

    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    int flags = fd_flags((int)fd);
    if (!(flags & VFS_WRONLY) && !(flags & VFS_RDWR))
        return -LINUX_EBADF;

    vfs_file_t* file = fd_get_file((int)fd);
    if (file == NULL) {
        for (uint64_t i = 0; i < count; ++i)
            putc(buf[i]);
        return (int64)count;
    }

    int wr = vfs_write(file, (const uint8_t*)buf, (uint32_t)count);
    if (wr < 0)
        return -LINUX_EBADF;

    return wr;
}

static int64 sys_writev(uint64_t fd, const linux_iovec_t* iov, uint64_t iovcnt) {
    if (iov == NULL)
        return -LINUX_EINVAL;

    int64 total = 0;
    for (uint64_t i = 0; i < iovcnt; ++i) {
        int64 written = sys_write(fd, (const char*)iov[i].iov_base, iov[i].iov_len);
        if (written < 0)
            return written;
        total += written;
    }

    return total;
}

static int64 sys_ioctl(uint64_t fd, uint64_t req, uint64_t arg) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    switch (req) {
        case LINUX_TIOCGWINSZ: {
            linux_winsize_t* ws = (linux_winsize_t*)arg;
            if (!ws)
                return -LINUX_EINVAL;
            ws->ws_row = 25;
            ws->ws_col = 80;
            ws->ws_xpixel = 0;
            ws->ws_ypixel = 0;
            return 0;
        }
        case LINUX_TCGETS: {
            linux_termios_t* tio = (linux_termios_t*)arg;
            if (!tio)
                return -LINUX_EINVAL;
            memset(tio, 0, sizeof(*tio));
            return 0;
        }
        case LINUX_TCSETS:
        case LINUX_TCSETSW:
        case LINUX_TCSETSF:
            return arg ? 0 : -LINUX_EINVAL;
        case LINUX_TIOCGPGRP:
            if (!arg)
                return -LINUX_EINVAL;
            *(int*)arg = 1;
            return 0;
        case LINUX_TIOCSPGRP:
            return arg ? 0 : -LINUX_EINVAL;
        default:
            return -LINUX_ENOTTY;
    }
}

static int64 sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    switch (cmd) {
        case LINUX_F_DUPFD: {
            if (arg >= STREAM_MAX_FDS)
                return -LINUX_EINVAL;
            for (int target = (int)arg; target < STREAM_MAX_FDS; ++target) {
                if (!fd_valid(target))
                    return fd_dup2((int)fd, target);
            }
            return -LINUX_ENFILE;
        }
        case LINUX_F_GETFD:
        case LINUX_F_SETFD:
            return 0;
        case LINUX_F_GETFL:
            return fd_flags((int)fd);
        case LINUX_F_SETFL:
            return 0;
        default:
            return -LINUX_ENOSYS;
    }
}

static int64 sys_access_common(int dirfd, const char* path, int mode) {
    (void)mode;
    if (!path)
        return -LINUX_EINVAL;
    vfs_stat_info_t info;
    return fill_vfs_stat_for_path_at(dirfd, path, &info) ? 0 : -LINUX_ENOENT;
}

static int64 sys_lseek(uint64_t fd, int64_t offset, uint64_t whence) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    uint32_t* pos = fd_pos_ptr((int)fd);
    if (!pos)
        return -LINUX_EINVAL;

    int64_t base = 0;
    switch (whence) {
        case LINUX_SEEK_SET:
            base = 0;
            break;
        case LINUX_SEEK_CUR:
            base = *pos;
            break;
        case LINUX_SEEK_END:
            base = fd_file_size((int)fd);
            break;
        default:
            return -LINUX_EINVAL;
    }

    int64_t new_pos = base + offset;
    if (new_pos < 0)
        return -LINUX_EINVAL;

    *pos = (uint32_t)new_pos;
    return new_pos;
}

static int64 sys_dup2(uint64_t oldfd, uint64_t newfd) {
    if (!fd_valid((int)oldfd))
        return -LINUX_EBADF;

    if (newfd >= STREAM_MAX_FDS)
        return -LINUX_EBADF;

    int rc = fd_dup2((int)oldfd, (int)newfd);
    return rc < 0 ? -LINUX_EBADF : (int64)rc;
}

static int64 sys_dup(uint64_t oldfd) {
    if (!fd_valid((int)oldfd))
        return -LINUX_EBADF;

    int newfd = fd_dup((int)oldfd);
    return newfd < 0 ? -LINUX_ENFILE : newfd;
}

static int64 sys_getcwd(char* buf, uint64_t size) {
    const char* cwd = vfs_getcwd();
    if (!buf || size == 0)
        return -LINUX_EINVAL;

    uint64_t len = strlen(cwd);
    if (len + 1 > size)
        return -LINUX_EINVAL;

    memcpy(buf, cwd, len + 1);
    return (int64)len;
}

static int64 sys_chdir(const char* path) {
    if (!path)
        return -LINUX_EINVAL;

    int rc = vfs_cd(path);
    if (rc != 0)
        return -LINUX_ENOENT;

    return 0;
}

static int64 sys_readlinkat(int dirfd, const char* path, char* buf, uint64_t bufsiz) {
    if (!path)
        return -LINUX_EINVAL;

    char resolved_path[256];
    if (!resolve_path_at(dirfd, path, resolved_path, sizeof(resolved_path)))
        return -LINUX_EINVAL;

    if (strcmp(resolved_path, "/proc/self/exe") == 0 || strcmp(resolved_path, "self/exe") == 0)
        return copy_readlink_result(current_exec_path, buf, bufsiz);

    return -LINUX_ENOENT;
}

static int64 sys_clock_gettime(uint64_t clockid, linux_timespec_t* tp) {
    if (!tp)
        return -LINUX_EINVAL;
    if (clockid != LINUX_CLOCK_REALTIME && clockid != LINUX_CLOCK_MONOTONIC)
        return -LINUX_EINVAL;

    int8 sec = 0, min = 0, hour = 0, day = 0, month = 0;
    int16 year = 0;
    update_system_time(&sec, &min, &hour, &day, &month, &year);

    tp->tv_sec = (hour * 3600) + (min * 60) + sec;
    tp->tv_nsec = 0;
    return 0;
}

static int64 sys_nanosleep(const linux_timespec_t* req, linux_timespec_t* rem) {
    (void)rem;
    if (!req)
        return -LINUX_EINVAL;
    if (req->tv_sec > 0)
        sleep((int)req->tv_sec);
    return 0;
}

static int64 sys_mmap(uint64_t addr, uint64_t length, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t off) {
    (void)addr;
    (void)prot;
    (void)flags;
    (void)fd;
    (void)off;

    uint64_t mapped = userland_mmap_anon(length);
    if (mapped == 0)
        return -LINUX_EINVAL;

    return (int64)mapped;
}

static int64 sys_mprotect(uint64_t addr, uint64_t length, uint64_t prot) {
    (void)addr;
    (void)length;
    (void)prot;
    return 0;
}

static int64 sys_brk(uint64_t requested_break) {
    return (int64)userland_brk(requested_break);
}

static int64 sys_munmap(uint64_t addr, uint64_t length) {
    (void)addr;
    (void)length;
    return 0;
}

static int64 sys_arch_prctl(uint64_t code, uint64_t addr) {
    switch (code) {
        case LINUX_ARCH_SET_FS:
            current_fs_base = addr;
            wrmsr64(IA32_FS_BASE_MSR, addr);
            return 0;
        case LINUX_ARCH_GET_FS:
            if (!addr)
                return -LINUX_EINVAL;
            *(uint64_t*)addr = rdmsr64(IA32_FS_BASE_MSR);
            return 0;
        default:
            return -LINUX_ENOSYS;
    }
}

static int64 sys_prlimit64(uint64_t pid, uint64_t resource, const linux_rlimit64_t* new_limit, linux_rlimit64_t* old_limit) {
    (void)resource;
    (void)new_limit;
    if (pid != 0 && pid != 1)
        return -LINUX_EINVAL;
    if (old_limit) {
        old_limit->rlim_cur = ~0ULL;
        old_limit->rlim_max = ~0ULL;
    }
    return 0;
}

static int64 sys_getrandom(void* buf, uint64_t buflen, uint64_t flags) {
    (void)flags;
    if (!buf)
        return -LINUX_EINVAL;
    uint8_t* out = (uint8_t*)buf;
    uint64_t state = rdtsc64();
    for (uint64_t i = 0; i < buflen; ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        out[i] = (uint8_t)(state >> (i & 7));
    }
    return (int64)buflen;
}

static int64 sys_execve(const char* target, char* const* argv, char* const* envp) {
    if (!target)
        return -LINUX_EINVAL;

    snprintf(current_exec_path, sizeof(current_exec_path), "%s", target);

    char** copied_argv = NULL;
    char** copied_envp = NULL;
    int argc = copy_user_string_array(argv, &copied_argv);
    if (argc < 0)
        return argc;

    int envc = copy_user_string_array(envp, &copied_envp);
    if (envc < 0) {
        free_copied_string_array(copied_argv, argc);
        return envc;
    }

    if (userland_exec(target, argc, (const char* const*)copied_argv, (const char* const*)copied_envp) == 0)
        return 0;

    free_copied_string_array(copied_argv, argc);
    free_copied_string_array(copied_envp, envc);

    int rc = execute_chain(target);
    return rc >= 0 ? rc : -LINUX_ENOEXEC;
}

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

static int64 sys_futex(uint32_t* uaddr, int op, uint32_t val,
                       const linux_timespec_t* timeout,
                       uint32_t* uaddr2, uint32_t val3)
{
    (void)timeout;
    (void)uaddr2;
    (void)val3;

    if (!uaddr)
        return -LINUX_EINVAL;

    switch (op & 0xF)
    {
        case FUTEX_WAIT:
            // If value doesn't match, return immediately
            if (*uaddr != val)
                return -LINUX_EAGAIN;

            // NO real blocking → just pretend we waited
            return 0;

        case FUTEX_WAKE:
            // pretend we woke threads
            return 1;

        default:
            return -LINUX_ENOSYS;
    }
}

struct passwd {
    char *pw_name;
    int pw_uid;
};

struct passwd fake_root = {
    .pw_name = "root",
    .pw_uid = 0,
};

struct passwd* getpwuid(int uid) {
    if (uid == 0)
        return &fake_root;
    return NULL;
}

// THIS IS FOR INTERRUPT 0X80
void int80_handler(InterruptFrame* frame)
{
    uint64_t ret = syscall_dispatch(
        frame->rax,
        frame->rdi,
        frame->rsi,
        frame->rdx,
        frame->rcx,   // arg4 (int 0x80 uses rcx)
        frame->r8,
        frame->r9
    );

    frame->rax = ret;
}

// THIS IS FOR SYSCALL INSTRUCTION
void syscall_handler(syscall_frame_t* f)
{
    if (f && (f->rax == LINUX_SYS_EXIT || f->rax == LINUX_SYS_EXIT_GROUP)) {
        if (userland_prepare_exit(f, f->rdi))
            return;
    }

    uint64_t ret = syscall_dispatch(
        f->rax,
        f->rdi,
        f->rsi,
        f->rdx,
        f->r10,   // ⚠️ DIFFERENT HERE
        f->r8,
        f->r9
    );

    f->rax = ret;
}

uint64_t syscall_dispatch (
    uint64_t nr,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5,
    uint64_t arg6
) {
    switch (nr)
    {
        case LINUX_SYS_READ:
            return sys_read(arg1, (char*)arg2, arg3);

        case LINUX_SYS_WRITE:
            return sys_write(arg1, (const char*)arg2, arg3);

        case LINUX_SYS_OPEN:
            return sys_open_common(LINUX_AT_FDCWD, (const char*)arg1, arg2, arg3);

        case LINUX_SYS_FSTAT:
            return sys_fstat(arg1, (linux_stat_t*)arg2);

        case LINUX_SYS_STAT:
            return sys_stat((const char*)arg1, (linux_stat_t*)arg2);

        case LINUX_SYS_LSTAT:
            return sys_newfstatat(
                LINUX_AT_FDCWD,
                (const char*)arg1,
                (linux_stat_t*)arg2,
                LINUX_AT_SYMLINK_NOFOLLOW
            );

        case LINUX_SYS_MPROTECT:
            return sys_mprotect(arg1, arg2, arg3);

        case LINUX_SYS_RT_SIGACTION:
        case LINUX_SYS_RT_SIGPROCMASK:
        case LINUX_SYS_SIGALTSTACK:
            return 0;

        case LINUX_SYS_ACCESS:
            return sys_access_common(LINUX_AT_FDCWD, (const char*)arg1, arg2);

        case LINUX_SYS_OPENAT:
            return sys_open_common((int)arg1, (const char*)arg2, arg3, arg4);

        case LINUX_SYS_CLOSE:
            return sys_close(arg1);

        case LINUX_SYS_LSEEK:
            return sys_lseek(arg1, (int64_t)arg2, arg3);

        case LINUX_SYS_MMAP:
            return sys_mmap(arg1, arg2, arg3, arg4, arg5, arg6);

        case LINUX_SYS_MUNMAP:
            return sys_munmap(arg1, arg2);

        case LINUX_SYS_BRK:
            return sys_brk(arg1);

        case LINUX_SYS_IOCTL:
            return sys_ioctl(arg1, arg2, arg3);

        case LINUX_SYS_WRITEV:
            return sys_writev(arg1, (linux_iovec_t*)arg2, arg3);

        case LINUX_SYS_DUP:
            return sys_dup(arg1);

        case LINUX_SYS_DUP2:
            return sys_dup2(arg1, arg2);

        case LINUX_SYS_NANOSLEEP:
            return sys_nanosleep((const linux_timespec_t*)arg1, (linux_timespec_t*)arg2);

        case LINUX_SYS_GETPID:
            return 1;

        case LINUX_SYS_EXECVE:
            return sys_execve((const char*)arg1, (char* const*)arg2, (char* const*)arg3);

        case LINUX_SYS_EXIT:
        case LINUX_SYS_EXIT_GROUP:
            return 0;

        case LINUX_SYS_GETCWD:
            return sys_getcwd((char*)arg1, arg2);

        case LINUX_SYS_FCNTL:
            return sys_fcntl(arg1, arg2, arg3);

        case LINUX_SYS_CHDIR:
            return sys_chdir((const char*)arg1);

        case LINUX_SYS_FORK:
            return 0;

        case LINUX_SYS_UNAME:
            return sys_uname((linux_utsname_t*)arg1);

        case LINUX_SYS_READLINK:
            return sys_readlinkat(LINUX_AT_FDCWD, (const char*)arg1, (char*)arg2, arg3);

        case LINUX_SYS_UMASK:
            return 022;

        case LINUX_SYS_GETUID:
        case LINUX_SYS_GETEUID:
        case LINUX_SYS_GETGID:
        case LINUX_SYS_GETEGID:
            return 0;

        case LINUX_SYS_GETPPID:
            return 1;

        case LINUX_SYS_ARCH_PRCTL:
            return sys_arch_prctl(arg1, arg2);

        case LINUX_SYS_GETTID:
            return 1;

        case LINUX_SYS_TGKILL:
            return 0;

        case LINUX_SYS_GETDENTS64:
            return sys_getdents64(arg1, (char*)arg2, arg3);

        case LINUX_SYS_SET_TID_ADDRESS:
            return 1;

        case LINUX_SYS_CLOCK_GETTIME:
            return sys_clock_gettime(arg1, (linux_timespec_t*)arg2);

        case LINUX_SYS_NEWFSTATAT:
            return sys_newfstatat((int)arg1, (const char*)arg2, (linux_stat_t*)arg3, (int)arg4);

        case LINUX_SYS_READLINKAT:
            return sys_readlinkat((int)arg1, (const char*)arg2, (char*)arg3, arg4);

        case LINUX_SYS_FACCESSAT:
            return sys_access_common((int)arg1, (const char*)arg2, (int)arg3);

        case LINUX_SYS_SET_ROBUST_LIST:
            return 0;

        case LINUX_SYS_PRLIMIT64:
            return sys_prlimit64(arg1, arg2, (const linux_rlimit64_t*)arg3, (linux_rlimit64_t*)arg4);

        case LINUX_SYS_GETRANDOM:
            return sys_getrandom((void*)arg1, arg2, arg3);

        case LINUX_SYS_STATX:
            return sys_statx((int)arg1, (const char*)arg2, (int)arg3, (unsigned int)arg4, (linux_statx_t*)arg5);

        case PRAD_MAGIC:
            info("Alive from userland", __FILE__);
            return 0;
        
        case LINUX_SYS_FUTEX:
            return sys_futex((uint32_t*)arg1, arg2, arg3,
                            (const linux_timespec_t*)arg4,
                            (uint32_t*)arg5, arg6);
        case 41: // getuid (32-bit ABI compatibility)
            return 0;
        case 19:
            {
                linux_iovec_t* iov = (linux_iovec_t*)arg2;
                if (arg3 > 0)
                    return sys_read(arg1, iov[0].iov_base, iov[0].iov_len);
                return 0;
            }

        case 7:
            {
                struct pollfd {
                    int fd;
                    short events;
                    short revents;
                };

                struct pollfd* fds = (struct pollfd*)arg1;

                for (int i = 0; i < arg2; i++)
                {
                    fds[i].revents = fds[i].events; // pretend ready
                }

                return arg2;
            }
        default:
            printf(linux_syscalls_prefix "Unknown, returning -ENOSYS for (%u)", nr);
            return -LINUX_ENOSYS;
    }
}
