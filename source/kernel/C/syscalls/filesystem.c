#include "internal.h"

#include <multitasking.h>
#include <net/net.h>
#include <rtc.h>
#include <tty.h>

// sys headers
#include <sys/dirent.h>
#include <sys/helper.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/utsname.h>

extern struct limine_framebuffer *framebuffer;
extern uint64 *font_address;

extern void reboot();
extern void shutdown();



#define LINUX_AF_INET 2
#define LINUX_SOCK_RAW 3
#define LINUX_IPPROTO_ICMP 1

typedef struct {
    uint16_t family;
    uint16_t port;
    uint32_t addr;
    uint8_t zero[8];
} linux_sockaddr_in_t;

// char current_exec_path[256] = "/";
uint64_t current_fs_base = 0;
uint32_t current_umask = 022;
uint64_t *clear_child_tid = NULL;
// char current_exec_argv_storage[32][128];
// const char* current_exec_argv[32];
// int current_exec_argc = 0;

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

static void iso_name_to_vfs_local(const char *in, uint8_t in_len, char *out, size_t out_sz) {
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

static void fill_stat_from_info(linux_stat_t *st, const vfs_stat_info_t *info) {
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

bool resolve_path_at(int dirfd, const char *path, char *out, size_t out_sz) {
    if (!path || !out)
        return false;

    if (path[0] == '/')
        return vfs_normalize_path(path, out, out_sz) == 0;

    if (dirfd == LINUX_AT_FDCWD || dirfd == 0)
        return vfs_normalize_path(path, out, out_sz) == 0;

    if (!fd_valid(dirfd))
        return false;

    const char *base = fd_get_path(dirfd);
    if (!base)
        base = vfs_getcwd();

    char joined[512];
    snprintf(joined, sizeof(joined), "%s/%s", base, path);
    return vfs_normalize_path(joined, out, out_sz) == 0;
}

bool fill_vfs_stat_for_path_at(int dirfd, const char *path, vfs_stat_info_t *info) {
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

    if (res.mnt->type == FS_SYS) {
        if (res.rel_path[0] == '\0') {
            info->is_dir = true;
            info->mode = LINUX_S_IFDIR | 0555;
            return true;
        }

        vfs_file_t sysf = {0};
        snprintf(sysf.rel_path, sizeof(sysf.rel_path), "%s", res.rel_path);
        sysf.mnt = res.mnt;
        if (sysfs_open(&sysf) != 0)
            return false;
        info->is_dir = sysfs_is_dir(res.rel_path);
        info->mode = (info->is_dir ? LINUX_S_IFDIR : LINUX_S_IFREG) | 0555;
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
        fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;
        fat16_dir_entry_t entry = {0};
        if (fat16_find_path(fs, res.rel_path, &entry) != 0)
            return false;
        info->is_dir = (entry.attr & 0x10) != 0;
        info->size = entry.filesize;
    } else if (res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
        fat32_dir_entry_t entry = {0};
        if (fat32_find_path(fs, res.rel_path, &entry) != FAT_OK)
            return false;
        info->is_dir = (entry.attr & FAT_ATTR_DIRECTORY) != 0;
        info->size = entry.file_size;
    } else if (res.mnt->type == FS_ISO9660) {
        iso9660_fs_t *fs = (iso9660_fs_t *)res.mnt->fs;
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

static bool fill_vfs_stat_for_fd(int fd, vfs_stat_info_t *info) {
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

    vfs_file_t *file = fd_get_file(fd);
    switch (file->mnt->type) {
        case FS_PROC:
        case FS_DEV:
            info->is_dir = (file->rel_path[0] == '\0');
            info->size = 0;
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
        case FS_EXT2:
            info->is_dir = file->f.ext2.is_dir != 0;
            info->size = file->f.ext2.inode.i_size;
            break;
        default:
            return false;
    }

    info->mode = info->is_dir ? (LINUX_S_IFDIR | 0755) : (LINUX_S_IFREG | 0644);
    return true;
}

uint64 copy_readlink_result(const char *target, char *buf, uint64_t bufsiz) {
    if (!target || !buf || bufsiz == 0)
        return -LINUX_EINVAL;

    uint64_t len = strlen(target);
    if (len > bufsiz)
        len = bufsiz;
    memcpy(buf, target, len);
    return (uint64)len;
}

bool path_is_loadable_elf(const char *path) {
    if (!path)
        return false;

    vfs_file_t file;
    if (vfs_open(path, VFS_RDONLY, &file) != 0)
        return false;

    uint8_t ident[4] = {0};
    int rd = vfs_read(&file, ident, sizeof(ident));
    vfs_close(&file);

    return rd == (int)sizeof(ident) && ident[0] == 0x7F && ident[1] == 'E' && ident[2] == 'L' && ident[3] == 'F';
}

bool should_route_to_toybox(const char *target) {
    // Bail out before logging anything for targets that were never a real
    // exec attempt (e.g. the default/unset current_exec_path == "/", or an
    // empty string). Without this, sys_fork() calling this before the very
    // first execve on a task spams "route -> /" for no reason.
    if (!target || target[0] == '\0' || strcmp(target, "/") == 0)
        return false;

    debug_printf("route -> %s\n", target);

    const char *applet = vfs_basename(target);
    if (!applet || applet[0] == '\0' || strcmp(applet, "toybox") == 0)
        return false;

    if (!path_is_loadable_elf("/bin/toybox"))
        return false;

    return !path_is_loadable_elf(target);
}

int build_toybox_argv(
    const char *target,
    int argc,
    char **copied_argv,
    const char *out_argv[32]) {
    const char *applet = vfs_basename(target);
    debug_printf("build_toybox_argv -> target = %s\n", target);
    int out_argc = 0;

    // argv[0] must be applet name
    out_argv[out_argc++] = applet;

    // copy everything from original argv[1..]
    for (int i = 1; i < argc && out_argc < 31; i++) {
        out_argv[out_argc++] = copied_argv[i];
    }

    out_argv[out_argc] = NULL;
    return out_argc;
}

static int emit_dirent(char *buf, uint64_t buflen, uint64_t *used, uint64_t ino, uint8_t type, const char *name, uint64_t next_off) {
    uint64_t name_len = strlen(name) + 1;
    uint64_t reclen = sizeof(linux_dirent64_t) + name_len;
    reclen = (reclen + 7) & ~7ULL;

    if (*used + reclen > buflen)
        return 0;

    linux_dirent64_t *ent = (linux_dirent64_t *)(buf + *used);
    ent->d_ino = ino;
    ent->d_off = next_off;
    ent->d_reclen = (uint16_t)reclen;
    ent->d_type = type;
    memcpy(ent->d_name, name, name_len);
    memset(((char *)ent) + sizeof(linux_dirent64_t) + name_len, 0, reclen - sizeof(linux_dirent64_t) - name_len);
    *used += reclen;
    return 1;
}

static uint32_t fat16_cluster_lba_local(fat16_fs_t *fs, uint16_t cluster) {
    if (cluster == FAT16_ROOT_CLUSTER)
        return fs->root_dir_start;
    return fs->data_start + ((uint32_t)(cluster - 2) * fs->bs.sectors_per_cluster);
}

static void fat32_short_name(const fat32_dir_entry_t *e, char *out, size_t out_sz) {
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

static uint8_t ext2_filetype_to_dt(uint8_t ft) {
    switch (ft) {
        case EXT2_FT_REG_FILE:
            return 8; /* DT_REG  */
        case EXT2_FT_DIR:
            return 4; /* DT_DIR  */
        case EXT2_FT_CHRDEV:
            return 2; /* DT_CHR  */
        case EXT2_FT_BLKDEV:
            return 6; /* DT_BLK  */
        case EXT2_FT_FIFO:
            return 1; /* DT_FIFO */
        case EXT2_FT_SOCK:
            return 12; /* DT_SOCK */
        case EXT2_FT_SYMLINK:
            return 10; /* DT_LNK  */
        default:
            return 0; /* DT_UNKNOWN */
    }
}

typedef struct {
    char *buf;
    uint64_t buflen;
    uint64_t *used;
    uint32_t *pos;
    uint64_t entry_index; /* resume point, from *pos */
    uint64_t idx;         /* running count of entries seen so far */
} ext2_getdents_ctx_t;

static int ext2_getdents_cb(uint32_t ino, uint8_t file_type, const char *name, void *user) {
    ext2_getdents_ctx_t *ctx = (ext2_getdents_ctx_t *)user;

    if (ctx->idx++ < ctx->entry_index)
        return 0; /* not at resume point yet, keep going */

    if (!emit_dirent(ctx->buf, ctx->buflen, ctx->used, ino,
            ext2_filetype_to_dt(file_type), name, ctx->idx))
        return 1; /* output buffer full, stop iteration */

    *ctx->pos = (uint32_t)ctx->idx;
    return 0;
}

uint64 sys_mkdirat(int dirfd, const char *path, int mode) {
    (void)mode;
    if (!path)
        return -LINUX_EINVAL;

    char resolved_path[256];
    if (!resolve_path_at(dirfd, path, resolved_path, sizeof(resolved_path)))
        return -LINUX_EINVAL;

    int rc = vfs_mkdir(resolved_path);
    if (rc == 0)
        return 0;

    switch (rc) {
        case EXT2_ERR_EXISTS:
            return -LINUX_EEXIST;
        case EXT2_ERR_NOSPACE:
            return -LINUX_ENOSPC;
        case EXT2_ERR_NOTDIR:
            return -LINUX_ENOTDIR;
        case EXT2_ERR_NOT_FOUND:
            return -LINUX_ENOENT;
        case EXT2_ERR_IO:
            return -LINUX_EIO;
        default:
            return -LINUX_EIO;
    }
}

uint64_t sys_unlink(const char *user_path) {
    char path[1024];

    if (strcpy(path, user_path) < 0)
        return -LINUX_EFAULT;

    int ret = vfs_unlink(path);

    if (ret < 0)
        return ret; // or translate to Linux errno if needed

    return 0;
}

int sys_getdents64(uint64_t fd, char *buf, uint64_t buflen) {
    if (!buf || buflen < sizeof(linux_dirent64_t))
        return -LINUX_EINVAL;
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    vfs_file_t *file = fd_get_file((int)fd);
    if (!file) {
        return -LINUX_ENOTDIR;
    }

    uint32_t *pos = fd_pos_ptr((int)fd);
    if (!pos)
        return -LINUX_ENOTDIR;

    uint64_t used = 0;
    uint64_t entry_index = *pos;

    if (file->flags & VFS_FILE_ROOT_DIR) {
        uint64_t idx = 0;

        for (int i = 1; i < mounted_partition_count; i++) {
            if (idx++ < entry_index)
                continue;

            mount_entry_t *m = &mounted_partitions[i];
            const char *name = vfs_basename(m->mount_point);

            if (!emit_dirent(
                    buf,
                    buflen,
                    &used,
                    path_inode_hash(m->mount_point),
                    4, // DT_DIR
                    name,
                    idx))
                break;

            *pos = (uint32_t)idx;
        }
    }

    if (file->mnt->type == FS_PROC) {
        uint64_t i = entry_index;
        const char *name;
        procfs_type_t type;

        while (procfs_getdent(file->rel_path, i, &name, &type)) {
            uint8_t d_type = (type == PROC_DIR) ? 4 : 8;

            if (!emit_dirent(buf, buflen, &used, path_inode_hash(name), d_type, name, i + 1))
                break;

            i++;
            *pos = (uint32_t)i;
        }

        return (int)used;
    }

    if (file->mnt->type == FS_SYS) {
        uint64_t i = entry_index;
        const char *name;
        procfs_type_t type;

        while (sysfs_getdent(file->rel_path, i, &name, &type)) {
            uint8_t d_type = (type == PROC_DIR) ? 4 : 8;

            if (!emit_dirent(buf, buflen, &used, path_inode_hash(name), d_type, name, i + 1))
                break;

            i++;
            *pos = (uint32_t)i;
        }

        return (int)used;
    }

    if (file->mnt->type == FS_DEV) {
        static const char *dev_entries[] = {"null", "zero", "random", "urandom", "klog", "syslog", "tty"};
        const uint64_t fixed = 7;
        uint64_t total = fixed;
        for (int i = 0; i < block_device_count; i++) {
            if (block_devices[i].present &&
                (block_devices[i].type == BLOCK_DEVICE_AHCI || block_devices[i].type == BLOCK_DEVICE_NVME))
                total++;
        }

        for (uint64_t i = entry_index; i < total; ++i) {
            const char *name = NULL;

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
        fat16_fs_t *fs = file->f.fat16.fs;
        uint16_t cluster = file->f.fat16.entry.first_cluster;
        uint8_t sector[SECTOR_SIZE];
        uint64_t idx = 0;

        if ((file->f.fat16.entry.attr & 0x10) == 0 && file->f.fat16.entry.first_cluster == 0 && file->f.fat16.entry.filesize == 0)
            cluster = FAT16_ROOT_CLUSTER;

        if (cluster == FAT16_ROOT_CLUSTER) {
            for (uint32_t s = 0; s < fs->root_dir_sectors; ++s) {
                ahci_read_sector(fs->portno, fs->root_dir_start + s, sector, 1);
                fat16_dir_entry_t *entries = (fat16_dir_entry_t *)sector;
                for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i) {
                    fat16_dir_entry_t *e = &entries[i];
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
                fat16_dir_entry_t *entries = (fat16_dir_entry_t *)sector;
                for (int i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i) {
                    fat16_dir_entry_t *e = &entries[i];
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
        fat32_fs_t *fs = file->f.fat32.fs;
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
                    fat32_dir_entry_t *e = (fat32_dir_entry_t *)(sector + off);
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
            cluster = (*(uint32_t *)(sector + ent_offset)) & 0x0FFFFFFF;
        }
        return (int)used;
    }

    if (file->mnt->type == FS_ISO9660) {
        iso9660_fs_t *fs = file->f.iso9660.fs;
        iso9660_dirent_t dir = file->f.iso9660.entry;
        if ((dir.flags & ISO9660_FLAG_DIR) == 0) {
            dir.extent_lba = fs->root_extent_lba;
            dir.size = fs->root_size;
            dir.flags = ISO9660_FLAG_DIR;
        }

        uint8_t *block = kmalloc(fs->logical_block_size);
        if (!block)
            return -LINUX_ENOMEM;

        uint64_t idx = 0;
        uint32_t blocks = (dir.size + fs->logical_block_size - 1) / fs->logical_block_size;
        for (uint32_t b = 0; b < blocks; ++b) {
            if (ahci_read_sector(fs->portno, dir.extent_lba + (b * (fs->logical_block_size / SECTOR_SIZE)), block, fs->logical_block_size / SECTOR_SIZE) != 0)
                break;
            uint32_t off = 0;
            while (off < fs->logical_block_size) {
                linux_iso9660_dir_record_t *r = (linux_iso9660_dir_record_t *)(block + off);
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

    if (file->mnt->type == FS_EXT2) {
        ext2_getdents_ctx_t ctx = {
            .buf = buf,
            .buflen = buflen,
            .used = &used,
            .pos = pos,
            .entry_index = entry_index,
            .idx = 0,
        };

        int rc = ext2_readdir(file->f.ext2.fs, file->f.ext2.ino, ext2_getdents_cb, &ctx);
        if (rc == EXT2_ERR_NOTDIR)
            return -LINUX_ENOTDIR;
        if (rc != EXT2_OK)
            return -1;

        return (int)used;
    }

    return -LINUX_ENOTDIR;
}

uint64 sys_open_common(int dirfd, const char *path, int flags, int mode) {
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

uint64 sys_close(uint64_t fd) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    sys_socket_close((int)fd);

    return fd_close((int)fd) == 0 ? 0 : -LINUX_EBADF;
}

uint64 sys_fstat(uint64_t fd, linux_stat_t *st) {
    vfs_stat_info_t info;
    if (!st)
        return -LINUX_EINVAL;
    if (!fill_vfs_stat_for_fd((int)fd, &info))
        return -LINUX_EBADF;
    fill_stat_from_info(st, &info);
    return 0;
}

uint64 sys_stat(const char *path, linux_stat_t *st) {
    if (!path || !st)
        return -LINUX_EINVAL;

    vfs_stat_info_t info;
    if (!fill_vfs_stat_for_path_at(LINUX_AT_FDCWD, path, &info))
        return -LINUX_ENOENT;

    fill_stat_from_info(st, &info);
    return 0;
}

uint64 sys_newfstatat(int dirfd, const char *path, linux_stat_t *st, int flags) {
    if (!st)
        return -LINUX_EINVAL;

    // Only ever act on the flags we actually support; silently ignore
    // anything else (AT_NO_AUTOMOUNT, AT_STATX_SYNC_*, etc.) instead of
    // rejecting the call outright.
    int known = flags & (LINUX_AT_SYMLINK_NOFOLLOW | LINUX_AT_EMPTY_PATH);

    if ((known & LINUX_AT_EMPTY_PATH) && path && path[0] == '\0')
        return sys_fstat((uint64_t)dirfd, st);

    if (!path)
        return -LINUX_EINVAL;
    vfs_stat_info_t info;
    if (!fill_vfs_stat_for_path_at(dirfd, path, &info))
        return -LINUX_ENOENT;
    fill_stat_from_info(st, &info);
    return 0;
}
uint64 sys_statx(int dirfd, const char *path, int flags, unsigned int mask, linux_statx_t *stx) {
    (void)mask;
    linux_stat_t st;
    if (!stx)
        return -LINUX_EINVAL;
    int compat_flags = flags & (LINUX_AT_SYMLINK_NOFOLLOW | LINUX_AT_EMPTY_PATH);
    uint64 rc = sys_newfstatat(dirfd, path, &st, compat_flags);

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

uint64 sys_read(uint64_t fd, char *buf, uint64_t count) {
    if (buf == NULL || count == 0)
        return 0;

    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    int flags = fd_flags((int)fd);
    if (!(flags & VFS_RDONLY) && !(flags & VFS_RDWR))
        return -LINUX_EBADF;

    vfs_file_t *file = fd_get_file((int)fd);
    if (fd == 0)
        return tty_read(buf, count);

    int rd = vfs_read(file, (uint8_t *)buf, (uint32_t)count);
    if (rd < 0)
        return -LINUX_EBADF;

    return rd;
}

uint64 sys_write(uint64_t fd, const char *buf, uint64_t count) {
    if (buf == NULL || count == 0)
        return 0;

    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    int flags = fd_flags((int)fd);
    if (!(flags & VFS_WRONLY) && !(flags & VFS_RDWR))
        return -LINUX_EBADF;

    vfs_file_t *file = fd_get_file((int)fd);
    if (file == NULL) {
        for (uint64_t i = 0; i < count; ++i)
            putc(buf[i]);
        return (uint64)count;
    }

    int wr = vfs_write(file, (const uint8_t *)buf, (uint32_t)count);
    if (wr < 0)
        return -LINUX_EBADF;

    return wr;
}

uint64 sys_writev(uint64_t fd, const linux_iovec_t *iov, uint64_t iovcnt) {
    if (iov == NULL)
        return -LINUX_EINVAL;

    uint64 total = 0;
    for (uint64_t i = 0; i < iovcnt; ++i) {
        uint64 written = sys_write(fd, (const char *)iov[i].iov_base, iov[i].iov_len);
        if (written < 0)
            return written;
        total += written;
    }

    return total;
}
