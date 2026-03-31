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
#include <syscalls.h>
#include <limine.h>
#include <keyboard.h>
#include <graphics.h>
#include <memory.h>
#include <stream.h>
#include <userland.h>
#include <filesystems/vfs.h>
#include <filesystems/layers/proc.h>
#include <filesystems/fat16.h>
#include <filesystems/fat32.h>
#include <filesystems/iso9660.h>
#include <executables/elf.h>
#include <ahci.h>
#include <rtc.h>
#include <heap.h>

extern struct limine_framebuffer *framebuffer;
extern int64* font_address;

extern int execute_chain(const char* line);
extern bool running; // from sh.c

#define LINUX_AT_FDCWD   (-100)

#define LINUX_O_RDONLY   0x0000
#define LINUX_O_WRONLY   0x0001
#define LINUX_O_RDWR     0x0002
#define LINUX_O_CREAT    0x0040
#define LINUX_O_TRUNC    0x0200
#define LINUX_O_APPEND   0x0400

#define LINUX_SEEK_SET   0
#define LINUX_SEEK_CUR   1
#define LINUX_SEEK_END   2

#define LINUX_EBADF      9
#define LINUX_EACCES     13
#define LINUX_EINVAL     22
#define LINUX_ENOEXEC    8
#define LINUX_ENOTTY     25
#define LINUX_ENOSYS     38
#define LINUX_ENFILE     23
#define LINUX_ENOENT     2
#define LINUX_ENOMEM     12
#define LINUX_ERANGE     34
#define LINUX_ENOTDIR    20

#define LINUX_S_IFMT     00170000
#define LINUX_S_IFDIR    0040000
#define LINUX_S_IFREG    0100000
#define LINUX_S_IFCHR    0020000

#define LINUX_AT_SYMLINK_NOFOLLOW 0x100
#define LINUX_AT_EMPTY_PATH       0x1000

#define LINUX_F_DUPFD    0
#define LINUX_F_GETFD    1
#define LINUX_F_SETFD    2
#define LINUX_F_GETFL    3
#define LINUX_F_SETFL    4

#define LINUX_TIOCGWINSZ 0x5413
#define LINUX_TCGETS     0x5401
#define LINUX_TCSETS     0x5402
#define LINUX_TCSETSW    0x5403
#define LINUX_TCSETSF    0x5404
#define LINUX_TIOCGPGRP  0x540F
#define LINUX_TIOCSPGRP  0x5410

#define LINUX_ARCH_SET_FS 0x1002
#define LINUX_ARCH_GET_FS 0x1003

#define LINUX_CLOCK_REALTIME 0
#define LINUX_CLOCK_MONOTONIC 1

#define IA32_FS_BASE_MSR 0xC0000100

#define PROC_FILE_COUNT 3

#define FW_SYS_GETC      0x1000
#define FW_SYS_GETC_NB   0x1001
#define FW_SYS_PUTC      0x1002
#define FW_SYS_LOGIN     0x1055

typedef struct {
    uint64_t iov_base;
    uint64_t iov_len;
} linux_iovec_t;

typedef struct {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} linux_utsname_t;

typedef struct {
    long tv_sec;
    long tv_nsec;
} linux_timespec_t;

typedef struct {
    uint32_t ws_row;
    uint32_t ws_col;
    uint32_t ws_xpixel;
    uint32_t ws_ypixel;
} linux_winsize_t;

typedef struct {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t c_line;
    uint8_t c_cc[19];
} linux_termios_t;

typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    linux_timespec_t st_atim;
    linux_timespec_t st_mtim;
    linux_timespec_t st_ctim;
    int64_t __unused[3];
} linux_stat_t;

typedef struct {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
} linux_dirent64_t;

typedef struct {
    uint64_t rlim_cur;
    uint64_t rlim_max;
} linux_rlimit64_t;

typedef struct {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t __spare0[1];
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    linux_timespec_t stx_atime;
    linux_timespec_t stx_btime;
    linux_timespec_t stx_ctime;
    linux_timespec_t stx_mtime;
    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t __spare2[14];
} linux_statx_t;

typedef struct {
    bool exists;
    bool is_dir;
    uint64_t size;
    uint32_t mode;
    uint64_t inode;
} vfs_stat_info_t;

static char current_exec_path[256] = "/";
static uint64_t current_fs_base = 0;

static void free_copied_string_array(char** arr, int count) {
    if (!arr)
        return;

    for (int i = 0; i < count; ++i)
        kfree(arr[i]);

    kfree(arr);
}

static int copy_user_string_array(char* const* user_arr, char*** out_arr) {
    if (!user_arr) {
        *out_arr = NULL;
        return 0;
    }

    char** copied = kmalloc(sizeof(char*) * 33);
    if (!copied)
        return -LINUX_ENOMEM;

    int count = 0;
    for (; count < 32; ++count) {
        const char* src = user_arr[count];
        if (!src) {
            copied[count] = NULL;
            *out_arr = copied;
            return count;
        }

        size_t len = strlen(src);
        copied[count] = kmalloc(len + 1);
        if (!copied[count]) {
            free_copied_string_array(copied, count);
            return -LINUX_ENOMEM;
        }

        memcpy(copied[count], src, len + 1);
    }

    copied[32] = NULL;
    *out_arr = copied;
    return 32;
}

static inline uint64_t syscall_arg4(InterruptFrame* frame) {
    return frame->r10 ? frame->r10 : frame->rcx;
}

static inline void wrmsr64(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr64(uint32_t msr) {
    uint32_t low = 0;
    uint32_t high = 0;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static uint64_t rdtsc64(void) {
    uint32_t low = 0;
    uint32_t high = 0;
    asm volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

static uint32_t path_inode_hash(const char* path) {
    uint32_t hash = 2166136261u;
    for (const unsigned char* p = (const unsigned char*)path; p && *p; ++p) {
        hash ^= *p;
        hash *= 16777619u;
    }
    return hash ? hash : 1;
}

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

static bool fill_vfs_stat_for_path(const char* path, vfs_stat_info_t* info) {
    if (!path || !info)
        return false;

    memset(info, 0, sizeof(*info));

    char norm[256];
    if (path[0] == '/')
        snprintf(norm, sizeof(norm), "%s", path);
    else
        snprintf(norm, sizeof(norm), "%s/%s", vfs_getcwd(), path);

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
    fd_table_init();

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
    fd_table_init();

    if (path == NULL)
        return -LINUX_EINVAL;

    if (dirfd != LINUX_AT_FDCWD && dirfd != 0)
        return -LINUX_EINVAL;

    int fd = fd_open(path, linux_flags_to_vfs(flags));
    if (fd == -1)
        return -LINUX_ENFILE;

    if (fd == -2) {
        return -LINUX_ENOENT;
    }

    return fd;
}

static int64 sys_close(uint64_t fd) {
    fd_table_init();

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
    if (dirfd != LINUX_AT_FDCWD && dirfd != 0)
        return -LINUX_EINVAL;

    vfs_stat_info_t info;
    if (!fill_vfs_stat_for_path(path, &info))
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
    fd_table_init();

    if (buf == NULL || count == 0)
        return 0;

    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    int flags = fd_flags((int)fd);
    if (!(flags & VFS_RDONLY) && !(flags & VFS_RDWR))
        return -LINUX_EBADF;

    vfs_file_t* file = fd_get_file((int)fd);
    if (file == NULL) {
        for (uint64_t i = 0; i < count; ++i) {
            char c = getc_nonblock();
            buf[i] = c;
            if (c == '\n' || c == '\r')
                return (int64)(i + 1);
        }
        return (int64)count;
    }

    int rd = vfs_read(file, (uint8_t*)buf, (uint32_t)count);
    if (rd < 0)
        return -LINUX_EBADF;

    return rd;
}

static int64 sys_write(uint64_t fd, const char* buf, uint64_t count) {
    fd_table_init();

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
    fd_table_init();
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
    if (dirfd != LINUX_AT_FDCWD && dirfd != 0)
        return -LINUX_EINVAL;

    vfs_stat_info_t info;
    return fill_vfs_stat_for_path(path, &info) ? 0 : -LINUX_ENOENT;
}

static int64 sys_lseek(uint64_t fd, int64_t offset, uint64_t whence) {
    fd_table_init();

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
    fd_table_init();

    if (!fd_valid((int)oldfd))
        return -LINUX_EBADF;

    if (newfd >= STREAM_MAX_FDS)
        return -LINUX_EBADF;

    int rc = fd_dup2((int)oldfd, (int)newfd);
    return rc < 0 ? -LINUX_EBADF : (int64)rc;
}

static int64 sys_dup(uint64_t oldfd) {
    fd_table_init();

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
    if (dirfd != LINUX_AT_FDCWD && dirfd != 0)
        return -LINUX_EINVAL;
    if (!path)
        return -LINUX_EINVAL;

    if (strcmp(path, "/proc/self/exe") == 0 || strcmp(path, "self/exe") == 0)
        return copy_readlink_result(current_exec_path, buf, bufsiz);

    return -LINUX_ENOENT;
}

static int64 sys_uname(linux_utsname_t* uts) {
    if (uts == NULL)
        return -LINUX_EINVAL;

    memset(uts, 0, sizeof(*uts));
    snprintf(uts->sysname, sizeof(uts->sysname), "FrostWing");
    snprintf(uts->nodename, sizeof(uts->nodename), "fwos");
    snprintf(uts->release, sizeof(uts->release), "0.1");
    snprintf(uts->version, sizeof(uts->version), "fw-kernel");
    snprintf(uts->machine, sizeof(uts->machine), "x86_64");
    snprintf(uts->domainname, sizeof(uts->domainname), "localdomain");

    return 0;
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

void invoke_syscall(int64 num) {
    asm volatile (
        "movq %0, %%rax\n\t"
        "int $0x80\n\t"
        :
        : "g" ((int64)num)
        : "rax"
    );
}

void syscalls_handler(InterruptFrame* frame){
    switch (frame->rax)
    {
        case LINUX_SYS_READ:
            frame->rax = sys_read(frame->rdi, (char*)frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_WRITE:
            frame->rax = sys_write(frame->rdi, (const char*)frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_OPEN:
            frame->rax = sys_open_common(LINUX_AT_FDCWD, (const char*)frame->rdi, frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_FSTAT:
            frame->rax = sys_fstat(frame->rdi, (linux_stat_t*)frame->rsi);
            break;
        case LINUX_SYS_MPROTECT:
            frame->rax = sys_mprotect(frame->rdi, frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_RT_SIGACTION:
        case LINUX_SYS_RT_SIGPROCMASK:
        case LINUX_SYS_SIGALTSTACK:
            frame->rax = 0;
            break;
        case LINUX_SYS_ACCESS:
            frame->rax = sys_access_common(LINUX_AT_FDCWD, (const char*)frame->rdi, frame->rsi);
            break;
        case LINUX_SYS_OPENAT:
            frame->rax = sys_open_common((int)frame->rdi, (const char*)frame->rsi, frame->rdx, syscall_arg4(frame));
            break;
        case LINUX_SYS_CLOSE:
            frame->rax = sys_close(frame->rdi);
            break;
        case LINUX_SYS_LSEEK:
            frame->rax = sys_lseek(frame->rdi, (int64_t)frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_MMAP:
            frame->rax = sys_mmap(frame->rdi, frame->rsi, frame->rdx, syscall_arg4(frame), frame->r8, frame->r9);
            break;
        case LINUX_SYS_MUNMAP:
            frame->rax = sys_munmap(frame->rdi, frame->rsi);
            break;
        case LINUX_SYS_BRK:
            frame->rax = sys_brk(frame->rdi);
            break;
        case LINUX_SYS_IOCTL:
            frame->rax = sys_ioctl(frame->rdi, frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_WRITEV:
            frame->rax = sys_writev(frame->rdi, (linux_iovec_t*)frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_DUP:
            frame->rax = sys_dup(frame->rdi);
            break;
        case LINUX_SYS_DUP2:
            frame->rax = sys_dup2(frame->rdi, frame->rsi);
            break;
        case LINUX_SYS_NANOSLEEP:
            frame->rax = sys_nanosleep((const linux_timespec_t*)frame->rdi, (linux_timespec_t*)frame->rsi);
            break;
        case LINUX_SYS_GETPID:
            frame->rax = 1;
            break;
        case LINUX_SYS_EXECVE:
            frame->rax = sys_execve((const char*)frame->rdi, (char* const*)frame->rsi, (char* const*)frame->rdx);
            break;
        case LINUX_SYS_EXIT:
        case LINUX_SYS_EXIT_GROUP: {
            int code = frame->rdi;
            printf(blue_color "\n[process exited with code %d]" reset_color, code);
            running = false;
            frame->rax = 0;
            break;
        }
        case LINUX_SYS_GETCWD:
            frame->rax = sys_getcwd((char*)frame->rdi, frame->rsi);
            break;
        case LINUX_SYS_FCNTL:
            frame->rax = sys_fcntl(frame->rdi, frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_CHDIR:
            frame->rax = sys_chdir((const char*)frame->rdi);
            break;
        case LINUX_SYS_UNAME:
            frame->rax = sys_uname((linux_utsname_t*)frame->rdi);
            break;
        case LINUX_SYS_READLINK:
            frame->rax = sys_readlinkat(LINUX_AT_FDCWD, (const char*)frame->rdi, (char*)frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_UMASK:
            frame->rax = 022;
            break;
        case LINUX_SYS_GETUID:
        case LINUX_SYS_GETEUID:
        case LINUX_SYS_GETGID:
        case LINUX_SYS_GETEGID:
            frame->rax = 0;
            break;
        case LINUX_SYS_GETPPID:
            frame->rax = 1;
            break;
        case LINUX_SYS_ARCH_PRCTL:
            frame->rax = sys_arch_prctl(frame->rdi, frame->rsi);
            break;
        case LINUX_SYS_GETTID:
            frame->rax = 1;
            break;
        case LINUX_SYS_TGKILL:
            frame->rax = 0;
            break;
        case LINUX_SYS_GETDENTS64:
            frame->rax = sys_getdents64(frame->rdi, (char*)frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_SET_TID_ADDRESS:
            frame->rax = 1;
            break;
        case LINUX_SYS_CLOCK_GETTIME:
            frame->rax = sys_clock_gettime(frame->rdi, (linux_timespec_t*)frame->rsi);
            break;
        case LINUX_SYS_NEWFSTATAT:
            frame->rax = sys_newfstatat((int)frame->rdi, (const char*)frame->rsi, (linux_stat_t*)frame->rdx, (int)syscall_arg4(frame));
            break;
        case LINUX_SYS_READLINKAT:
            frame->rax = sys_readlinkat((int)frame->rdi, (const char*)frame->rsi, (char*)frame->rdx, syscall_arg4(frame));
            break;
        case LINUX_SYS_FACCESSAT:
            frame->rax = sys_access_common((int)frame->rdi, (const char*)frame->rsi, (int)frame->rdx);
            break;
        case LINUX_SYS_SET_ROBUST_LIST:
            frame->rax = 0;
            break;
        case LINUX_SYS_PRLIMIT64:
            frame->rax = sys_prlimit64(frame->rdi, frame->rsi, (const linux_rlimit64_t*)frame->rdx, (linux_rlimit64_t*)syscall_arg4(frame));
            break;
        case LINUX_SYS_GETRANDOM:
            frame->rax = sys_getrandom((void*)frame->rdi, frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_STATX:
            frame->rax = sys_statx((int)frame->rdi, (const char*)frame->rsi, (int)frame->rdx, (unsigned int)syscall_arg4(frame), (linux_statx_t*)frame->r8);
            break;
        case FW_SYS_GETC:
            frame->rax = getc();
            break;
        case FW_SYS_PUTC:
            printfnoln("%c", (char)frame->rdi);
            frame->rax = 0;
            break;
        case FW_SYS_LOGIN:
            frame->rax = login_request((char*)frame->rdi, frame->rsi);
            break;
        case PRAD_MAGIC:
            info("Alive from userland", __FILE__);
            frame->rax = 0;
            break;
        default:
            warn(linux_syscalls_prefix "Unknown, returning -ENOSYS", __FILE__);
            debug_printf(linux_syscalls_prefix "Unknown : requested %u\n", frame->rax);
            frame->rax = -LINUX_ENOSYS;
            break;
    }

    outb(0x20, 0x20); // End PIC Master
    outb(0xA0, 0x20); // End PIC Slave
}
