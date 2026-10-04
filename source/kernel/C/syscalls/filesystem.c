#include <syscalls/internal.h>

#include <multitasking.h>
#include <net/net.h>
#include <rtc.h>
#include <tty.h>
#include <ahci.h>
#include <filesystems/layers/dev.h>

// sys headers
#include <sys/dirent.h>
#include <sys/helper.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <filesystems/layers/sys.h>

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

uint64_t current_fs_base = 0;
uint32_t current_umask = 022;
uint64_t *clear_child_tid = NULL;

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
    st->st_rdev = info->rdev ? info->rdev : (info->is_dir ? 0 : 1);
    st->st_size = (int64_t)info->size;
    st->st_blksize = 512;
    st->st_blocks = (info->size + 511) / 512;
    st->st_atim = info->atim;
    st->st_mtim = info->mtim;
    st->st_ctim = info->ctim;
}

static uint64_t linux_makedev(uint32_t major, uint32_t minor) {
    return ((uint64_t)(minor & 0xffU)) |
        ((uint64_t)(major & 0xfffU) << 8) |
        ((uint64_t)(minor & ~0xffU) << 12) |
        ((uint64_t)(major & ~0xfffU) << 32);
}

static uint64_t fat_datetime_to_unix(uint16_t date, uint16_t time) {
    uint32_t year = 1980U + ((date >> 9) & 0x7FU);
    uint32_t month = (date >> 5) & 0x0FU;
    uint32_t day = date & 0x1FU;
    uint32_t hour = (time >> 11) & 0x1FU;
    uint32_t minute = (time >> 5) & 0x3FU;
    uint32_t second = (time & 0x1FU) * 2U;

    static const uint8_t month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 59)
        return 0;

    uint64_t days = 0;
    for (uint32_t y = 1970; y < year; y++)
        days += ((y % 4 == 0) && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    for (uint32_t m = 1; m < month; m++) {
        days += month_days[m - 1];
        if (m == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
            days++;
    }
    days += day - 1;
    return days * 86400ULL + hour * 3600ULL + minute * 60ULL + second;
}

static void set_stat_times(vfs_stat_info_t *info, uint64_t atime, uint64_t mtime, uint64_t ctime) {
    info->atim.tv_sec = (int64_t)atime;
    info->mtim.tv_sec = (int64_t)mtime;
    info->ctim.tv_sec = (int64_t)ctime;
}

static void unix_to_fat_datetime(uint64_t unix_time, uint16_t *date, uint16_t *time) {
    if (unix_time < 315532800ULL)
        unix_time = 315532800ULL;
    if (unix_time > UINT32_MAX)
        unix_time = UINT32_MAX;
    uint64_t days = unix_time / 86400ULL;
    uint32_t seconds = (uint32_t)(unix_time % 86400ULL);
    uint32_t year = 1970;
    while (days >= (((year % 4U) == 0U &&
        ((year % 100U) != 0U || (year % 400U) == 0U)) ? 366U : 365U)) {
        uint32_t year_days = ((year % 4U) == 0U &&
            ((year % 100U) != 0U || (year % 400U) == 0U)) ? 366U : 365U;
        days -= year_days;
        year++;
    }
    static const uint8_t month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint32_t month = 0;
    while (month < 11) {
        uint32_t length = month_days[month];
        if (month == 1 && (year % 4U) == 0U &&
            ((year % 100U) != 0U || (year % 400U) == 0U))
            length++;
        if (days < length)
            break;
        days -= length;
        month++;
    }
    if (year < 1980)
        year = 1980;
    if (year > 2107)
        year = 2107;

    uint32_t hour = seconds / 3600U;
    uint32_t minute = (seconds / 60U) % 60U;
    uint32_t second = (seconds % 60U) / 2U;
    *date = (uint16_t)(((year - 1980U) << 9) |
        ((month + 1U) << 5) | ((uint32_t)days + 1U));
    *time = (uint16_t)((hour << 11) | (minute << 5) | second);
}

uint64 sys_set_file_times(const char *path, uint64_t atime, uint64_t mtime) {
    if (!path)
        return -LINUX_EINVAL;
    vfs_mount_res_t res;
    if (vfs_resolve_mount(path, &res) != 0)
        return -LINUX_ENOENT;
    if (!res.rel_path[0])
        return -LINUX_EISDIR;

    if (res.mnt->type == FS_EXT2) {
        if (atime > UINT32_MAX || mtime > UINT32_MAX)
            return -LINUX_EINVAL;
        int rc = ext2_set_times((ext2_fs_t *)res.mnt->fs, res.rel_path,
            (uint32_t)atime, (uint32_t)mtime);
        if (rc == EXT2_ERR_NOT_FOUND)
            return -LINUX_ENOENT;
        return rc == EXT2_OK ? 0 : -LINUX_EIO;
    }

    uint16_t atime_date, atime_time, mtime_date, mtime_time;
    unix_to_fat_datetime(atime, &atime_date, &atime_time);
    unix_to_fat_datetime(mtime, &mtime_date, &mtime_time);
    if (res.mnt->type == FS_FAT16) {
        fat16_file_t file;
        fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;
        if (fat16_open(fs, res.rel_path, &file) != 0)
            return -LINUX_ENOENT;
        file.entry.last_access_date = atime_date;
        file.entry.last_mod_date = mtime_date;
        file.entry.last_mod_time = mtime_time;
        int rc = file.parent_cluster == FAT16_ROOT_CLUSTER ?
            (fat16_update_root_entry(fs, &file.entry), 0) :
            fat16_update_dir_entry(fs, file.parent_cluster, &file.entry);
        return rc == 0 ? 0 : -LINUX_EIO;
    }
    if (res.mnt->type == FS_FAT32) {
        fat32_file_t file;
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
        if (fat32_open(fs, res.rel_path, &file) != 0)
            return -LINUX_ENOENT;
        file.entry.access_date = atime_date;
        file.entry.write_date = mtime_date;
        file.entry.write_time = mtime_time;
        fat32_update_entry(fs, &file.entry);
        return 0;
    }
    if (res.mnt->type == FS_ISO9660)
        return -LINUX_EROFS;
    return -LINUX_EACCES;
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

static bool fill_vfs_stat_for_path_at_impl(int dirfd, const char *path, vfs_stat_info_t *info, bool nofollow) {
    if (!path || !info)
        return false;

    memset(info, 0, sizeof(*info));
    uint64_t now = rtc_get_unix_time();
    info->atim.tv_sec = (int64_t)now;
    info->mtim.tv_sec = (int64_t)now;
    info->ctim.tv_sec = (int64_t)now;

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

        int proc_path_type = procfs_path_is_dir(res.rel_path);
        if (proc_path_type < 0)
            return false;
        info->is_dir = proc_path_type > 0;
        info->mode = (info->is_dir ? LINUX_S_IFDIR : LINUX_S_IFREG) | 0555;
        info->size = 0;
        return true;
    }

    if (res.mnt->type == FS_SYS) {
        if (res.rel_path[0] == '\0') {
            info->is_dir = true;
            info->mode = LINUX_S_IFDIR | 0555;
            return true;
        }

        if (sysfs_is_symlink(res.rel_path)) {
            if (nofollow) {
                info->mode = LINUX_S_IFLNK | 0777;
                return true;
            }

            char target[256];
            int target_len = sysfs_readlink(res.rel_path, target, sizeof(target) - 1);
            if (target_len < 0)
                return false;
            target[target_len] = '\0';

            char link_path[512];
            const char *last_slash = strrchr(norm, '/');
            size_t parent_len = last_slash ? (size_t)(last_slash - norm) : 0;
            if (parent_len + (size_t)target_len + 2 > sizeof(link_path))
                return false;
            memcpy(link_path, norm, parent_len);
            link_path[parent_len] = '/';
            memcpy(link_path + parent_len + 1, target, (size_t)target_len + 1);
            return fill_vfs_stat_for_path_at_impl(LINUX_AT_FDCWD, link_path, info, false);
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
        
        bool is_block = false;
        int disk_id = -1;
        
        for (int i = 0; i < block_device_count; i++) {
            block_device_info_t *dev = &block_devices[i];
            if (!dev->present)
                continue;
            if (strcmp(dev->name, res.rel_path) == 0) {
                is_block = true;
                disk_id = i;
                break;
            }
        }
        
        if (!is_block) {
            for (int i = 0; i < general_partition_count; i++) {
                if (strcmp(ahci_partitions[i].name, res.rel_path) == 0) {
                    is_block = true;
                    break;
                }
            }
        }
        
        if (is_block) {
            info->mode = LINUX_S_IFBLK | 0666;
            if (disk_id >= 0) {
                block_device_info_t *dev = &block_devices[disk_id];
                info->size = dev->total_sectors * dev->sector_size;
                info->rdev = linux_makedev(
                    dev->type == BLOCK_DEVICE_NVME ? 259U : 8U,
                    (uint32_t)disk_id * 16U);
            } else {
                for (int i = 0; i < general_partition_count; i++) {
                    general_partition_t *partition = &ahci_partitions[i];
                    if (strcmp(partition->name, res.rel_path) == 0) {
                        int device_id = (int)partition->ahci_port;
                        block_device_info_t *dev = block_get_device(device_id);
                        if (dev) {
                            uint32_t partition_number = 1;
                            for (int j = 0; j < i; j++) {
                                if (ahci_partitions[j].ahci_port == partition->ahci_port)
                                    partition_number++;
                            }
                            info->rdev = linux_makedev(
                                dev->type == BLOCK_DEVICE_NVME ? 259U : 8U,
                                (uint32_t)device_id * 16U + partition_number);
                            info->size = partition->sector_count * dev->sector_size;
                        }
                        break;
                    }
                }
            }
        } else {
            info->mode = LINUX_S_IFCHR | 0666;
            info->size = 0;
        }
        
        if (strcmp(res.rel_path, "tty") == 0)
            info->rdev = 0x500; /* /dev/tty: major 5, minor 0 */
        else if (strcmp(res.rel_path, "tty1") == 0)
            info->rdev = 0x401; /* /dev/tty1: major 4, minor 1 */
        return true;
    }

    /* Root of a real-fs mount point */
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
        uint64_t created = fat_datetime_to_unix(entry.creation_date, entry.creation_time);
        set_stat_times(info,
            fat_datetime_to_unix(entry.last_access_date, 0),
            fat_datetime_to_unix(entry.last_mod_date, entry.last_mod_time),
            created);
    } else if (res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
        fat32_dir_entry_t entry = {0};
        if (fat32_find_path(fs, res.rel_path, &entry) != FAT_OK)
            return false;
        info->is_dir = (entry.attr & FAT_ATTR_DIRECTORY) != 0;
        info->size = entry.file_size;
        uint64_t created = fat_datetime_to_unix(entry.creation_date, entry.creation_time);
        set_stat_times(info,
            fat_datetime_to_unix(entry.access_date, 0),
            fat_datetime_to_unix(entry.write_date, entry.write_time),
            created);
    } else if (res.mnt->type == FS_ISO9660) {
        iso9660_fs_t *fs = (iso9660_fs_t *)res.mnt->fs;
        iso9660_dirent_t entry = {0};
        if (iso9660_find_path(fs, res.rel_path, &entry) != 0)
            return false;
        info->is_dir = (entry.flags & ISO9660_FLAG_DIR) != 0;
        info->size = entry.size;
    } else if (res.mnt->type == FS_EXT2) {
        /* THE FIX: ext2 was missing entirely */
        ext2_fs_t *fs = (ext2_fs_t *)res.mnt->fs;
        uint32_t ino = 0;
        ext2_inode_t inode;
        memset(&inode, 0, sizeof(inode));
        if (ext2_find_path_ex(fs, res.rel_path, &ino, &inode, !nofollow) != EXT2_OK)
            return false;

        info->inode = ino;
        info->is_dir = (inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR;
        info->size = inode.i_size;
        /* ext2 i_mode uses the same bits as Linux st_mode */
        info->mode = inode.i_mode;
        set_stat_times(info, inode.i_atime, inode.i_mtime, inode.i_ctime);
        if ((info->mode & 0xF000) == 0)
            info->mode |= info->is_dir ? LINUX_S_IFDIR : LINUX_S_IFREG;
        return true;
    } else {
        return false;
    }

    info->mode = (info->is_dir ? LINUX_S_IFDIR | 0755 : LINUX_S_IFREG | 0644);
    return true;
}

bool fill_vfs_stat_for_path_at(int dirfd, const char *path, vfs_stat_info_t *info) {
    return fill_vfs_stat_for_path_at_impl(dirfd, path, info, false);
}

bool fill_vfs_lstat_for_path_at(int dirfd, const char *path, vfs_stat_info_t *info) {
    return fill_vfs_stat_for_path_at_impl(dirfd, path, info, true);
}

static bool fill_vfs_stat_for_fd(int fd, vfs_stat_info_t *info) {
    if (!info || !fd_valid(fd))
        return false;

    memset(info, 0, sizeof(*info));
    info->exists = true;
    info->inode = (uint64_t)(fd + 1);

    if (fd_is_pipe(fd)) {
        info->mode = LINUX_S_IFIFO | 0600;
        return true;
    }

    if (sys_socket_is_fd(fd)) {
        info->mode = LINUX_S_IFSOCK | 0666;
        return true;
    }

    if (fd <= STDERR || fd_get_file(fd) == NULL) {
        info->is_dir = false;
        info->size = 0;
        info->mode = LINUX_S_IFCHR | 0666;
        info->inode = (uint64_t)(fd + 3);
        info->rdev = 0x401; /* /dev/tty1: major 4, minor 1 */
        return true;
    }

    vfs_file_t *file = fd_get_file(fd);
    switch (file->mnt->type) {
        case FS_PROC:
            info->is_dir = (file->rel_path[0] == '\0') ||
                        (procfs_path_is_dir(file->rel_path) > 0);
            info->size = 0;
            break;
        case FS_SYS:
            info->is_dir = (file->rel_path[0] == '\0') ||
                        sysfs_is_dir(file->rel_path);
            info->size = 0;
            break;
        case FS_DEV:
            info->is_dir = (file->rel_path[0] == '\0');
            info->size = 0;
            break;
        case FS_FAT16:
            info->is_dir = (file->f.fat16.entry.attr & 0x10) != 0;
            info->size = file->f.fat16.entry.filesize;
            set_stat_times(info,
                fat_datetime_to_unix(file->f.fat16.entry.last_access_date, 0),
                fat_datetime_to_unix(file->f.fat16.entry.last_mod_date,
                    file->f.fat16.entry.last_mod_time),
                fat_datetime_to_unix(file->f.fat16.entry.creation_date,
                    file->f.fat16.entry.creation_time));
            break;
        case FS_FAT32:
            info->is_dir = (file->f.fat32.entry.attr & FAT_ATTR_DIRECTORY) != 0;
            info->size = file->f.fat32.entry.file_size;
            set_stat_times(info,
                fat_datetime_to_unix(file->f.fat32.entry.access_date, 0),
                fat_datetime_to_unix(file->f.fat32.entry.write_date,
                    file->f.fat32.entry.write_time),
                fat_datetime_to_unix(file->f.fat32.entry.creation_date,
                    file->f.fat32.entry.creation_time));
            break;
        case FS_ISO9660:
            info->is_dir = (file->f.iso9660.entry.flags & ISO9660_FLAG_DIR) != 0;
            info->size = file->f.iso9660.entry.size;
            break;
        case FS_EXT2:
            info->is_dir = file->f.ext2.is_dir != 0;
            info->size = file->f.ext2.inode.i_size;
            info->inode = file->f.ext2.ino;
            info->mode = file->f.ext2.inode.i_mode;
            set_stat_times(info, file->f.ext2.inode.i_atime,
                file->f.ext2.inode.i_mtime, file->f.ext2.inode.i_ctime);
            return true;
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
    int hide_lost_found;  /* set when listing the fs root */
} ext2_getdents_ctx_t;

static int ext2_getdents_cb(uint32_t ino, uint8_t file_type, const char *name, void *user) {
    ext2_getdents_ctx_t *ctx = (ext2_getdents_ctx_t *)user;

    /* Must come BEFORE idx++, so hidden entries don't consume an offset.
     * Otherwise the resume position in *pos would be off by one. */
    if (ctx->hide_lost_found && strcmp(name, "lost+found") == 0)
        return 0;

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
    return sys_unlinkat(LINUX_AT_FDCWD, user_path, 0);
}

uint64 sys_unlinkat(int dirfd, const char *user_path, int flags) {
    if (!user_path)
        return -LINUX_EFAULT;
    if (flags & ~LINUX_AT_REMOVEDIR)
        return -LINUX_EINVAL;

    char path[256];
    if (!resolve_path_at(dirfd, user_path, path, sizeof(path)))
        return -LINUX_EINVAL;

    vfs_stat_info_t info;
    if (!fill_vfs_stat_for_path_at(LINUX_AT_FDCWD, path, &info))
        return -LINUX_ENOENT;
    if (strcmp(path, "/") == 0)
        return -LINUX_EBUSY;

    int rc;
    if (flags & LINUX_AT_REMOVEDIR) {
        if (!info.is_dir)
            return -LINUX_ENOTDIR;
        rc = vfs_rmdir(path);
    } else {
        if (info.is_dir)
            return -LINUX_EISDIR;
        rc = vfs_unlink(path);
    }

    if (rc == 0)
        return 0;
    if (rc == EXT2_ERR_NOT_FOUND || rc == FAT_ERR_NOT_FOUND)
        return -LINUX_ENOENT;
    if (rc == EXT2_ERR_NOTEMPTY || rc == FAT_ERR_NOT_EMPTY)
        return -LINUX_ENOTEMPTY;
    if (rc == EXT2_ERR_ISDIR || rc == FAT_ERR_IS_DIR)
        return -LINUX_EISDIR;
    if (rc == EXT2_ERR_NOTDIR || rc == FAT_ERR_NOT_DIR)
        return -LINUX_ENOTDIR;
    return -LINUX_EIO;
}

uint64 sys_rename(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath)
        return -LINUX_EFAULT;

    char old_norm[256], new_norm[256];
    if (!resolve_path_at(LINUX_AT_FDCWD, oldpath, old_norm, sizeof(old_norm)) ||
        !resolve_path_at(LINUX_AT_FDCWD, newpath, new_norm, sizeof(new_norm)))
        return -LINUX_EINVAL;

    vfs_mount_res_t old_mount, new_mount;
    if (vfs_resolve_mount(old_norm, &old_mount) != 0 ||
        vfs_resolve_mount(new_norm, &new_mount) != 0)
        return -LINUX_ENOENT;
    vfs_stat_info_t source_info;
    if (!fill_vfs_stat_for_path_at(LINUX_AT_FDCWD, old_norm, &source_info))
        return -LINUX_ENOENT;
    if (strcmp(old_norm, new_norm) == 0)
        return 0;
    if (old_mount.mnt != new_mount.mnt)
        return -LINUX_EXDEV;
    if (vfs_path_is_dir(new_norm) == 1)
        return -LINUX_EISDIR;

    return vfs_mv(old_norm, new_norm) == 0 ? 0 : -LINUX_EIO;
}

uint64 sys_copy_file_range(uint64_t in_fd, int64_t *off_in,
    uint64_t out_fd, int64_t *off_out, uint64_t len, uint64_t flags) {
    if (flags != 0)
        return -LINUX_EINVAL;
    if (!fd_valid((int)in_fd) || !fd_valid((int)out_fd))
        return -LINUX_EBADF;
    if (in_fd == out_fd)
        return -LINUX_EINVAL;
    if (!(fd_flags((int)in_fd) & (VFS_RDONLY | VFS_RDWR)) ||
        !(fd_flags((int)out_fd) & (VFS_WRONLY | VFS_RDWR)))
        return -LINUX_EBADF;

    vfs_file_t *in_file = fd_get_file((int)in_fd);
    vfs_file_t *out_file = fd_get_file((int)out_fd);
    uint32_t *in_pos = fd_pos_ptr((int)in_fd);
    uint32_t *out_pos = fd_pos_ptr((int)out_fd);
    if (!in_file || !out_file || !in_file->mnt || !out_file->mnt ||
        !in_pos || !out_pos)
        return -LINUX_EBADF;
    if (in_file == out_file)
        return -LINUX_EINVAL;

    if (in_file->mnt->type == FS_DEV || in_file->mnt->type == FS_PROC ||
        in_file->mnt->type == FS_SYS || out_file->mnt->type == FS_DEV ||
        out_file->mnt->type == FS_PROC || out_file->mnt->type == FS_SYS)
        return -LINUX_EINVAL;

    uint32_t saved_in_pos = *in_pos;
    uint32_t saved_out_pos = *out_pos;
    if (off_in && (*off_in < 0 || (uint64_t)*off_in > UINT32_MAX))
        return -LINUX_EINVAL;
    if (off_out && (*off_out < 0 || (uint64_t)*off_out > UINT32_MAX))
        return -LINUX_EINVAL;

    uint64_t read_offset = off_in ? (uint64_t)*off_in : saved_in_pos;
    uint64_t write_offset = off_out ? (uint64_t)*off_out : saved_out_pos;
    if (read_offset > UINT32_MAX || write_offset > UINT32_MAX)
        return -LINUX_EINVAL;

    uint64_t max_read = UINT32_MAX - read_offset;
    uint64_t max_write = UINT32_MAX - write_offset;
    if (len > max_read)
        len = max_read;
    if (len > max_write)
        len = max_write;
    if (len == 0)
        return 0;

    const char *in_path = fd_get_path((int)in_fd);
    const char *out_path = fd_get_path((int)out_fd);
    if (in_file->mnt == out_file->mnt && in_path && out_path &&
        strcmp(in_path, out_path) == 0 &&
        read_offset <= write_offset + len - 1 &&
        write_offset <= read_offset + len - 1)
        return -LINUX_EINVAL;

    uint8_t *buffer = kmalloc(4096);
    if (!buffer)
        return -LINUX_ENOMEM;

    uint64 copied = 0;
    uint64 error = 0;
    while (copied < len) {
        uint64 remaining = len - copied;
        uint32_t chunk = remaining > 4096 ? 4096 : (uint32_t)remaining;

        if (off_in)
            *in_pos = (uint32_t)(read_offset + copied);
        if (off_out)
            *out_pos = (uint32_t)(write_offset + copied);

        int rd = vfs_read(in_file, buffer, chunk);
        if (rd < 0) {
            error = -LINUX_EIO;
            break;
        }
        if (rd == 0)
            break;

        int wr = vfs_write(out_file, buffer, (uint32_t)rd);
        if (wr < 0) {
            error = -LINUX_EIO;
            break;
        }
        copied += (uint32_t)wr;
        if (wr != rd)
            break;
    }

    if (off_in) {
        *off_in = (int64_t)(read_offset + copied);
        *in_pos = saved_in_pos;
    } else
        *in_pos = saved_in_pos + (uint32_t)copied;

    if (off_out) {
        *off_out = (int64_t)(write_offset + copied);
        *out_pos = saved_out_pos;
    } else
        *out_pos = saved_out_pos + (uint32_t)copied;

    kfree(buffer);
    return copied ? copied : error;
}

uint64 sys_mount(const char *source, const char *target, const char *filesystem,
    uint64_t flags, const void *data) {
    (void)data;
    if (!source || !target)
        return -LINUX_EFAULT;
    if (flags != 0)
        return -LINUX_EOPNOTSUPP;

    char target_norm[256];
    if (!resolve_path_at(LINUX_AT_FDCWD, target, target_norm, sizeof(target_norm)))
        return -LINUX_EINVAL;

    if (filesystem && *filesystem &&
        strcmp(filesystem, "auto") != 0) {
        bool supported =
            strcmp(filesystem, "proc") == 0 ||
            strcmp(filesystem, "devtmpfs") == 0 ||
            strcmp(filesystem, "dev") == 0 ||
            strcmp(filesystem, "sysfs") == 0 ||
            strcmp(filesystem, "sys") == 0 ||
            strcmp(filesystem, "vfat") == 0 ||
            strcmp(filesystem, "fat16") == 0 ||
            strcmp(filesystem, "fat32") == 0 ||
            strcmp(filesystem, "ext2") == 0 ||
            strcmp(filesystem, "iso9660") == 0;
        if (!supported)
            return -LINUX_EOPNOTSUPP;

        if (strcmp(filesystem, "proc") == 0 &&
            strcmp(source, "proc") != 0 && strcmp(source, "none") != 0)
            return -LINUX_EINVAL;
        if ((strcmp(filesystem, "dev") == 0 || strcmp(filesystem, "devtmpfs") == 0) &&
            strcmp(source, "dev") != 0 && strcmp(source, "devtmpfs") != 0 &&
            strcmp(source, "none") != 0)
            return -LINUX_EINVAL;
        if ((strcmp(filesystem, "sys") == 0 || strcmp(filesystem, "sysfs") == 0) &&
            strcmp(source, "sys") != 0 && strcmp(source, "sysfs") != 0 &&
            strcmp(source, "none") != 0)
            return -LINUX_EINVAL;

        if (strcmp(source, "proc") != 0 && strcmp(source, "dev") != 0 &&
            strcmp(source, "sys") != 0 && strcmp(source, "none") != 0 &&
            strcmp(source, "devtmpfs") != 0 && strcmp(source, "sysfs") != 0) {
            general_partition_t *part = search_general_partition(
                strncmp(source, "/dev/", 5) == 0 ? source + 5 : source);
            if (!part)
                return -LINUX_ENOENT;
            partition_fs_type_t expected = part->fs_type;
            bool matches =
                (expected == FS_FAT16 && (strcmp(filesystem, "fat16") == 0 ||
                    strcmp(filesystem, "vfat") == 0)) ||
                (expected == FS_FAT32 && (strcmp(filesystem, "fat32") == 0 ||
                    strcmp(filesystem, "vfat") == 0)) ||
                (expected == FS_EXT2 && strcmp(filesystem, "ext2") == 0) ||
                (expected == FS_ISO9660 && strcmp(filesystem, "iso9660") == 0);
            if (!matches)
                return -LINUX_EINVAL;
        }
    }

    const char *mount_source = source;
    if (filesystem &&
        ((strcmp(filesystem, "devtmpfs") == 0 &&
                (strcmp(source, "devtmpfs") == 0 || strcmp(source, "none") == 0)) ||
         (strcmp(filesystem, "sysfs") == 0 &&
                (strcmp(source, "sysfs") == 0 || strcmp(source, "none") == 0))))
        mount_source = strcmp(filesystem, "devtmpfs") == 0 ? "dev" : "sys";
    if (filesystem && strcmp(filesystem, "proc") == 0 && strcmp(source, "none") == 0)
        mount_source = "proc";

    return vfs_mount(mount_source, target_norm, true) == 0 ? 0 : -LINUX_EINVAL;
}

uint64 sys_umount2(const char *target, int flags) {
    if (!target)
        return -LINUX_EFAULT;
    if (flags != 0)
        return -LINUX_EOPNOTSUPP;

    char target_norm[256];
    if (!resolve_path_at(LINUX_AT_FDCWD, target, target_norm, sizeof(target_norm)))
        return -LINUX_EINVAL;
    if (strcmp(target_norm, "/") == 0)
        return -LINUX_EPERM;

    return vfs_umount(target_norm, true) == 0 ? 0 : -LINUX_EINVAL;
}
static int getdents_iso9660(vfs_file_t *file, char *buf, uint64_t buflen, uint32_t *pos) {
    iso9660_fs_t *fs = file->f.iso9660.fs;
    iso9660_dirent_t dir = file->f.iso9660.entry;

    if ((dir.flags & ISO9660_FLAG_DIR) == 0) {
        dir.extent_lba = fs->root_extent_lba;
        dir.size = fs->root_size;
        dir.flags = ISO9660_FLAG_DIR;
    }

    uint32_t lbs = fs->logical_block_size;
    uint32_t spb = lbs / SECTOR_SIZE; /* sectors per logical block */

    uint8_t *block = kmalloc(lbs);
    if (!block)
        return -LINUX_ENOMEM;

    uint64_t used = 0;
    uint64_t idx = 0;
    uint64_t entry_index = *pos;
    uint32_t blocks = (dir.size + lbs - 1) / lbs;

    for (uint32_t b = 0; b < blocks; ++b) {
        /* extent_lba is in LOGICAL BLOCKS -> convert to sectors, add partition base */
        uint32_t lba = fs->partition_lba + (dir.extent_lba + b) * spb;
        if (ahci_read_sector(fs->portno, lba, block, spb) != 0)
            break;

        uint32_t off = 0;
        while (off < lbs) {
            linux_iso9660_dir_record_t *r = (linux_iso9660_dir_record_t *)(block + off);
            if (r->length == 0)
                break;

            /* skip the "." (0x00) and ".." (0x01) records */
            if (r->name_len == 1 && (uint8_t)r->name[0] <= 1) {
                off += r->length;
                continue;
            }

            if (idx++ >= entry_index) {
                char name[128];

                if (fs->joliet) {
                    /* UTF-16BE -> bytes, same rules as iso9660.c */
                    size_t oi = 0;
                    const uint8_t *in = (const uint8_t *)r->name;
                    for (uint8_t i = 0; (uint8_t)(i + 1) < r->name_len && oi + 1 < sizeof(name); i += 2) {
                        uint16_t wc = ((uint16_t)in[i] << 8) | in[i + 1];
                        if (wc == ';')
                            break;
                        name[oi++] = (wc <= 0xFF) ? (char)wc : '?';
                    }
                    if (oi > 0 && name[oi - 1] == '.')
                        oi--;
                    name[oi] = '\0';
                } else {
                    iso_name_to_vfs_local(r->name, r->name_len, name, sizeof(name));
                }

                if (!emit_dirent(buf, buflen, &used, path_inode_hash(name),
                        (r->flags & ISO9660_FLAG_DIR) ? 4 : 8, name, idx))
                    goto done;

                *pos = (uint32_t)idx;
            }

            off += r->length;
        }
    }

done:
    kfree(block);
    return (int)used;
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

    /* NOTE: the old VFS_FILE_ROOT_DIR block is gone. That flag collided with
     * VFS_RDONLY (both 0x1), so it fired for every read-only open. Mount
     * points already exist as real directories on the parent fs (vfs_mount
     * enforces this), so the underlying fs listing already contains them. */

    if (file->mnt->type == FS_PROC) {
        uint64_t i = entry_index;
        const char *name;
        procfs_type_t type;

        while (procfs_getdent(file->rel_path, i, &name, &type)) {
            uint8_t d_type = type == PROC_DIR ? 4 :
                (type == PROC_SYMLINK ? 10 : 8);

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
            uint8_t d_type = type == PROC_DIR ? 4 :
                (type == PROC_SYMLINK ? 10 : 8);

            if (!emit_dirent(buf, buflen, &used, path_inode_hash(name), d_type, name, i + 1))
                break;

            i++;
            *pos = (uint32_t)i;
        }

        return (int)used;
    }

    if (file->mnt->type == FS_DEV) {
        static const char *dev_entries[] = {
            "null", "zero", "random", "urandom", "klog", "syslog",
            "tty", "tty1", "rtc", "rtc0"};
        const uint64_t fixed = sizeof(dev_entries) / sizeof(dev_entries[0]);
        uint64_t total = fixed;
        for (int i = 0; i < block_device_count; i++) {
            if (block_devices[i].present)
                total++;
        }
        for (int i = 0; i < general_partition_count; i++) {
            if (block_get_device((int)ahci_partitions[i].ahci_port))
                total++;
        }

        for (uint64_t i = entry_index; i < total; ++i) {
            const char *name = NULL;
            char dynamic_name[64];

            if (i < fixed) {
                name = dev_entries[i];
                if (!emit_dirent(buf, buflen, &used, path_inode_hash(name),
                        8, name, i + 1))
                    break;
                *pos = (uint32_t)(i + 1);
                continue;
            } else {
                uint64_t disk_idx = i - fixed;
                uint64_t seen = 0;
                for (int j = 0; j < block_device_count; j++) {
                    if (!block_devices[j].present)
                        continue;
                    if (seen++ == disk_idx) {
                        name = block_devices[j].name;
                        break;
                    }
                }
                if (!name) {
                    disk_idx -= seen;
                    seen = 0;
                    for (int j = 0; j < general_partition_count; j++) {
                        if (!block_get_device((int)ahci_partitions[j].ahci_port))
                            continue;
                        if (seen++ == disk_idx) {
                            strncpy(dynamic_name, ahci_partitions[j].name,
                                sizeof(dynamic_name) - 1);
                            dynamic_name[sizeof(dynamic_name) - 1] = '\0';
                            name = dynamic_name;
                            break;
                        }
                    }
                }
            }

            if (!name)
                continue;

            if (!emit_dirent(buf, buflen, &used, path_inode_hash(name), 6, name, i + 1))
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

    if (file->mnt->type == FS_ISO9660)
        return getdents_iso9660(file, buf, buflen, pos);

    if (file->mnt->type == FS_EXT2) {
        ext2_getdents_ctx_t ctx = {
            .buf = buf,
            .buflen = buflen,
            .used = &used,
            .pos = pos,
            .entry_index = entry_index,
            .idx = 0,
            .hide_lost_found = (file->f.ext2.ino == EXT2_ROOT_INO),
        };

        int rc = ext2_readdir(file->f.ext2.fs, file->f.ext2.ino, ext2_getdents_cb, &ctx);
        if (rc == EXT2_ERR_NOTDIR)
            return -LINUX_ENOTDIR;
        if (rc != EXT2_OK)
            return -LINUX_EIO;

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

uint64 sys_ftruncate(uint64_t fd, int64_t length) {
    if (length < 0)
        return -LINUX_EINVAL;
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;
    if (fd_is_eventfd((int)fd))
        return -LINUX_EINVAL;

    int flags = fd_flags((int)fd);
    if (!(flags & VFS_WRONLY) && !(flags & VFS_RDWR))
        return -LINUX_EBADF;

    vfs_file_t *file = fd_get_file((int)fd);
    if (!file || !file->mnt)
        return -LINUX_EINVAL;
    if (length > UINT32_MAX)
        return -LINUX_EINVAL;

    uint32_t new_size = (uint32_t)length;
    uint32_t old_size = fd_file_size((int)fd);
    uint32_t *position = fd_pos_ptr((int)fd);
    if (!position)
        return -LINUX_EINVAL;
    uint32_t old_position = *position;

    if (file->mnt->type == FS_EXT2 && file->f.ext2.is_dir)
        return -LINUX_EISDIR;
    if (file->mnt->type == FS_FAT16 &&
        (file->f.fat16.entry.attr & 0x10))
        return -LINUX_EISDIR;
    if (file->mnt->type == FS_FAT32 && file->f.fat32.is_dir)
        return -LINUX_EISDIR;
    if (file->mnt->type == FS_ISO9660)
        return -LINUX_EROFS;
    if (file->mnt->type != FS_EXT2 && file->mnt->type != FS_FAT16 &&
        file->mnt->type != FS_FAT32)
        return -LINUX_EINVAL;

    if (new_size > old_size) {
        static const uint8_t zeros[4096] = {0};
        *position = old_size;
        while (*position < new_size) {
            uint32_t chunk = new_size - *position;
            if (chunk > sizeof(zeros))
                chunk = sizeof(zeros);
            int written = vfs_write(file, zeros, chunk);
            if (written <= 0) {
                *position = old_position;
                if (written == 0 || written == EXT2_ERR_NOSPACE)
                    return -LINUX_ENOSPC;
                return -LINUX_EIO;
            }
        }
        *position = old_position;
        return 0;
    }

    if (new_size == old_size)
        return 0;

    int rc;
    switch (file->mnt->type) {
        case FS_EXT2:
            rc = ext2_truncate_size(file->f.ext2.fs, &file->f.ext2, new_size);
            if (rc != EXT2_OK)
                return rc == EXT2_ERR_NOSPACE ? -LINUX_ENOSPC : -LINUX_EIO;
            break;
        case FS_FAT16:
            rc = fat16_truncate(&file->f.fat16, new_size);
            if (rc != FAT_OK)
                return -LINUX_EIO;
            if (file->f.fat16.parent_cluster == 0)
                fat16_update_root_entry(file->f.fat16.fs, &file->f.fat16.entry);
            else if (fat16_update_dir_entry(file->f.fat16.fs,
                         file->f.fat16.parent_cluster, &file->f.fat16.entry) != 0)
                return -LINUX_EIO;
            break;
        case FS_FAT32:
            rc = fat32_truncate(&file->f.fat32, new_size);
            if (rc != FAT_OK)
                return -LINUX_EIO;
            break;
        default:
            return -LINUX_EINVAL;
    }

    *position = old_position;
    return 0;
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
    bool nofollow = (known & LINUX_AT_SYMLINK_NOFOLLOW) != 0;
    if (!(nofollow ? fill_vfs_lstat_for_path_at(dirfd, path, &info) :
              fill_vfs_stat_for_path_at(dirfd, path, &info)))
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
    int64_t rc = (int64_t)sys_newfstatat(dirfd, path, &st, compat_flags);

    if (rc < 0)
        return (uint64)rc;

    memset(stx, 0, sizeof(*stx));
    stx->stx_mask = 0x7ff; /* STATX_BASIC_STATS */
    stx->stx_blksize = (uint32_t)st.st_blksize;
    stx->stx_nlink = (uint32_t)st.st_nlink;
    stx->stx_uid = st.st_uid;
    stx->stx_gid = st.st_gid;
    stx->stx_mode = (uint16_t)st.st_mode;
    stx->stx_ino = st.st_ino;
    stx->stx_size = st.st_size;
    stx->stx_blocks = st.st_blocks;
    stx->stx_atime = st.st_atim;
    stx->stx_mtime = st.st_mtim;
    stx->stx_ctime = st.st_ctim;
    stx->stx_rdev_major =
        (uint32_t)(((st.st_rdev >> 8) & 0xfffU) |
                   ((st.st_rdev >> 32) & ~0xfffU));
    stx->stx_rdev_minor =
        (uint32_t)((st.st_rdev & 0xffU) |
                   ((st.st_rdev >> 12) & 0xffffff00U));
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

    if (fd_is_pipe((int)fd)) {
        int rd = fd_pipe_read((int)fd, buf, count > INT32_MAX ? INT32_MAX : (size_t)count);
        return rd < 0 ? (uint64)(int64_t)rd : (uint64)rd;
    }

    if (fd_is_eventfd((int)fd)) {
        if (count != sizeof(uint64_t))
            return -LINUX_EINVAL;

        uint64_t value;
        for (;;) {
            int rd = fd_eventfd_read((int)fd, &value);
            if (rd > 0) {
                memcpy(buf, &value, sizeof(value));
                return sizeof(value);
            }
            if (rd < 0)
                return -LINUX_EBADF;
            if (fd_eventfd_nonblocking((int)fd))
                return -LINUX_EAGAIN;
            multitasking_yield();
        }
    }

    if (sys_socket_is_fd((int)fd))
        return sys_socket_read(fd, buf, count);

    vfs_file_t *file = fd_get_file((int)fd);
    if (fd_is_tty((int)fd))
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

    if (fd_is_pipe((int)fd)) {
        int wr = fd_pipe_write((int)fd, buf, count > INT32_MAX ? INT32_MAX : (size_t)count);
        return wr < 0 ? -LINUX_EPIPE : (uint64)wr;
    }

    if (fd_is_eventfd((int)fd)) {
        if (count != sizeof(uint64_t))
            return -LINUX_EINVAL;

        uint64_t value;
        memcpy(&value, buf, sizeof(value));
        if (value == UINT64_MAX)
            return -LINUX_EINVAL;

        for (;;) {
            int wr = fd_eventfd_write((int)fd, value);
            if (wr > 0)
                return sizeof(value);
            if (wr < 0)
                return -LINUX_EBADF;
            if (fd_eventfd_nonblocking((int)fd))
                return -LINUX_EAGAIN;
            multitasking_yield();
        }
    }

    if (sys_socket_is_fd((int)fd))
        return sys_socket_write(fd, buf, count);

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
        int64_t written = (int64_t)sys_write(fd, (const char *)iov[i].iov_base, iov[i].iov_len);
        if (written < 0)
            return total > 0 ? total : (uint64)written;
        total += (uint64)written;
        if ((uint64_t)written < iov[i].iov_len)
            break; /* short write, stop like Linux does */
    }

    return total;
}

static int64_t statfs_magic_for_type(partition_fs_type_t type) {
    switch (type) {
        case FS_PROC:    return LINUX_PROC_SUPER_MAGIC;
        case FS_SYS:     return LINUX_SYSFS_MAGIC;
        case FS_DEV:     return LINUX_TMPFS_MAGIC;
        case FS_FAT16:
        case FS_FAT32:   return LINUX_MSDOS_SUPER_MAGIC;
        case FS_ISO9660: return LINUX_ISOFS_SUPER_MAGIC;
        case FS_EXT2:    return LINUX_EXT2_SUPER_MAGIC;
        default:         return 0;
    }
}

static int statfs_count_fat16_free(fat16_fs_t *fs, uint64_t clusters,
    uint64_t *free_clusters) {
    uint8_t sector[SECTOR_SIZE];
    const uint64_t entries_per_sector = SECTOR_SIZE / sizeof(uint16_t);
    uint64_t end = clusters + 2;
    *free_clusters = 0;

    for (uint64_t first = 2; first < end;) {
        uint64_t sector_index =
            (first * sizeof(uint16_t)) / SECTOR_SIZE;
        if (ahci_read_sector(fs->portno, fs->fat_start + sector_index,
                sector, 1) != 0)
            return -1;

        uint64_t sector_first = sector_index * entries_per_sector;
        uint64_t begin = first > sector_first ? first : sector_first;
        uint64_t limit = sector_first + entries_per_sector;
        if (limit > end)
            limit = end;
        for (uint64_t cluster = begin; cluster < limit; ++cluster) {
            uint16_t value;
            memcpy(&value, sector + (cluster - sector_first) * sizeof(value),
                sizeof(value));
            if (value == 0)
                ++*free_clusters;
        }
        first = limit;
    }
    return 0;
}

static int statfs_count_fat32_free(fat32_fs_t *fs, uint64_t clusters,
    uint64_t *free_clusters) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    const uint64_t entries_per_sector = FAT32_SECTOR_SIZE / sizeof(uint32_t);
    uint64_t end = clusters + 2;
    *free_clusters = 0;

    for (uint64_t first = 2; first < end;) {
        uint64_t sector_index = first / entries_per_sector;
        if (ahci_read_sector(fs->portno, fs->fat_start_lba + sector_index,
                sector, 1) != 0)
            return -1;

        uint64_t sector_first = sector_index * entries_per_sector;
        uint64_t begin = first > sector_first ? first : sector_first;
        uint64_t limit = sector_first + entries_per_sector;
        if (limit > end)
            limit = end;
        for (uint64_t cluster = begin; cluster < limit; ++cluster) {
            uint32_t value;
            memcpy(&value, sector + (cluster - sector_first) * sizeof(value),
                sizeof(value));
            if ((value & 0x0FFFFFFF) == FAT32_CLUSTER_FREE)
                ++*free_clusters;
        }
        first = limit;
    }
    return 0;
}

static int statfs_backing_geometry(mount_entry_t *mnt, uint64_t *sectors,
    uint32_t *sector_size) {
    general_partition_t *part = search_general_partition(mnt->part_name);
    if (part) {
        *sectors = part->sector_count;
        block_device_info_t *device = block_get_device((int)part->ahci_port);
        *sector_size = device && device->sector_size ? device->sector_size : SECTOR_SIZE;
        return 0;
    }

    for (int i = 0; i < block_device_count; ++i) {
        block_device_info_t *device = &block_devices[i];
        if (!device->present || strcmp(device->name, mnt->part_name) != 0)
            continue;
        if (!device->sector_size)
            return -1;
        *sectors = device->total_sectors;
        *sector_size = device->sector_size;
        return 0;
    }
    return -1;
}

int fill_statfs_for_mount(mount_entry_t *mnt, linux_statfs_t *out) {
    memset(out, 0, sizeof(*out));

    if (!mnt) {
        out->f_bsize = 512;
        out->f_frsize = 512;
        out->f_namelen = 255;
        return 0;
    }

    out->f_type = statfs_magic_for_type(mnt->type);
    out->f_bsize = 512;
    out->f_frsize = 512;
    out->f_namelen = 255;

    if (mnt->type == FS_FAT16) {
        fat16_fs_t *fs = mnt->fs;
        if (!fs || fs->bs.bytes_per_sector != SECTOR_SIZE ||
            !fs->bs.sectors_per_cluster || !fs->bs.num_fats)
            return -1;

        uint64_t total_sectors = fs->bs.total_sectors_short ?
            fs->bs.total_sectors_short : fs->bs.total_sectors_long;
        uint64_t root_sectors =
            ((uint64_t)fs->bs.max_root_dir_entries * 32 + SECTOR_SIZE - 1) /
            SECTOR_SIZE;
        uint64_t overhead = (uint64_t)fs->bs.reserved_sectors +
            (uint64_t)fs->bs.num_fats * fs->bs.sectors_per_fat + root_sectors;
        if (total_sectors < overhead)
            return -1;

        uint64_t clusters =
            (total_sectors - overhead) / fs->bs.sectors_per_cluster;
        uint64_t free_clusters;
        if (statfs_count_fat16_free(fs, clusters, &free_clusters) != 0)
            return -1;

        uint64_t cluster_size =
            (uint64_t)SECTOR_SIZE * fs->bs.sectors_per_cluster;
        out->f_bsize = cluster_size;
        out->f_frsize = cluster_size;
        out->f_blocks = clusters;
        out->f_bfree = free_clusters;
        out->f_bavail = free_clusters;
        return 0;
    }

    if (mnt->type == FS_FAT32) {
        fat32_fs_t *fs = mnt->fs;
        if (!fs || fs->bpb.bytes_per_sector != FAT32_SECTOR_SIZE ||
            !fs->sectors_per_cluster)
            return -1;

        uint64_t total_sectors = fs->bpb.total_sectors_32;
        uint64_t overhead = (uint64_t)fs->bpb.reserved_sectors +
            (uint64_t)fs->bpb.fat_count * fs->bpb.fat_size_32;
        if (total_sectors < overhead)
            return -1;

        uint64_t clusters =
            (total_sectors - overhead) / fs->sectors_per_cluster;
        uint64_t free_clusters;
        if (statfs_count_fat32_free(fs, clusters, &free_clusters) != 0)
            return -1;

        uint64_t cluster_size =
            (uint64_t)FAT32_SECTOR_SIZE * fs->sectors_per_cluster;
        out->f_bsize = cluster_size;
        out->f_frsize = cluster_size;
        out->f_blocks = clusters;
        out->f_bfree = free_clusters;
        out->f_bavail = free_clusters;
        return 0;
    }

    if (mnt->type == FS_EXT2) {
        ext2_fs_t *fs = mnt->fs;
        if (!fs || !fs->block_size)
            return -1;
        out->f_bsize = fs->block_size;
        out->f_frsize = fs->block_size;
        out->f_blocks = fs->sb.s_blocks_count;
        out->f_bfree = fs->sb.s_free_blocks_count;
        out->f_bavail = fs->sb.s_free_blocks_count > fs->sb.s_r_blocks_count ?
            fs->sb.s_free_blocks_count - fs->sb.s_r_blocks_count : 0;
        out->f_files = fs->sb.s_inodes_count;
        out->f_ffree = fs->sb.s_free_inodes_count;
        return 0;
    }

    if (mnt->type == FS_PROC || mnt->type == FS_SYS || mnt->type == FS_DEV)
        return 0;

    uint64_t sectors;
    uint32_t sector_size;
    if (statfs_backing_geometry(mnt, &sectors, &sector_size) != 0)
        return 0;

    if (mnt->type == FS_ISO9660) {
        iso9660_fs_t *fs = mnt->fs;
        uint32_t block_size = fs && fs->logical_block_size ?
            fs->logical_block_size : ISO9660_SECTOR_SIZE;
        if (sectors > UINT64_MAX / sector_size)
            return -1;
        uint64_t bytes = sectors * sector_size;
        out->f_bsize = block_size;
        out->f_frsize = block_size;
        out->f_blocks = bytes / block_size;
        return 0;
    }

    if (sectors > UINT64_MAX / sector_size)
        return -1;
    out->f_bsize = sector_size;
    out->f_frsize = sector_size;
    out->f_blocks = sectors;
    return 0;
}