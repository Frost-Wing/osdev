/**
 * @file vfs.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The VFS code for FrostWing.
 * @version 0.1
 * @date 2025-12-31
 *
 * @copyright Copyright (c) Pradosh 2025
 *
 */

#include <ahci.h>
#include <basics.h>
#include <debugger.h>
#include <filesystems/ext2.h>
#include <filesystems/fat16.h>
#include <filesystems/fat32.h>
#include <filesystems/iso9660.h>
#include <filesystems/layers/dev.h>
#include <filesystems/layers/proc.h>
#include <filesystems/vfs.h>
#include <graphics.h>
#include <heap.h>
#include <memory.h>
#include <strings.h>
#include <klog.h>

char vfs_cwd[256] = "/";
uint16_t vfs_cwd_cluster = 0;

static int path_matches_mount(const char *path, const char *mount) {
    if (!path || !mount)
        return 0;

    /* Root mount matches everything absolute */
    if (strcmp(mount, "/") == 0)
        return path[0] == '/';

    size_t mlen = strlen(mount);
    if (strncmp(path, mount, mlen) != 0)
        return 0;

    return path[mlen] == '\0' || path[mlen] == '/';
}

int vfs_resolve_mount(const char *path, vfs_mount_res_t *out) {
    if (!path || !out) {
        eprintf("resolve_mount: invalid arguments");
        return -1;
    }

    mount_entry_t *best = NULL;
    int best_len = -1;

    for (int i = 0; i < mounted_partition_count; i++) {
        mount_entry_t *m = &mounted_partitions[i];
        int len = strlen(m->mount_point);

        if (path_matches_mount(path, m->mount_point)) {
            if (len > best_len) {
                best = m;
                best_len = len;
            }
        }
    }

    if (!best) {
        if (path)
            eprintf("resolve_mount: no mount matches path: %s", path);
        else
            eprintf("resolve_mount: no mount matches the given path.");
        return -2;
    }

    const char *rel = path + best_len;
    if (*rel == '/')
        rel++;

    out->mnt = best;
    out->rel_path = rel;
    return 0;
}

int vfs_normalize_path(const char *in, char *out, size_t out_sz) {
    if (!in || !out || out_sz < 2)
        return -1;

    memset(out, 0, out_sz);
    char tmp[256];

    // Start with absolute or relative
    if (in[0] != '/') {
        snprintf(tmp, sizeof(tmp), "%s/%s", vfs_cwd, in);
    } else {
        strncpy(tmp, in, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
    }

    int oi = 0;
    const char *p = tmp;

    while (*p) {
        // Skip extra '/'
        while (*p == '/')
            p++;
        if (!*p)
            break;

        // Handle '.'
        if (!strncmp(p, ".", 1) && (p[1] == '/' || p[1] == '\0')) {
            p += 1;
            continue;
        }

        // Handle '..'
        if (!strncmp(p, "..", 2) && (p[2] == '/' || p[2] == '\0')) {
            // Remove last directory from 'out'
            if (oi > 1)
                oi--; // step back from trailing '/'
            while (oi > 0 && out[oi - 1] != '/')
                oi--;
            if (oi == 0)
                oi = 1; // always keep leading '/'
            p += 2;
            continue;
        }

        // Add '/' before next component
        if (oi == 0 || out[oi - 1] != '/')
            out[oi++] = '/';

        // Copy next component
        while (*p && *p != '/') {
            if ((size_t)oi >= out_sz - 1) {
                out[out_sz - 1] = '\0';
                return -2;
            }
            out[oi++] = *p++;
        }
    }

    if (oi == 0)
        out[oi++] = '/';
    out[oi] = '\0';
    return 0;
}

int vfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size) {
    if (!file || !buf)
        return -1;

    if (!(file->flags & VFS_RDONLY) &&
        !(file->flags & VFS_RDWR)) {
        eprintf("read: file not opened for reading");
        return -2;
    }

    switch (file->mnt->type) {
        case FS_PROC:
            return procfs_read(file, buf, size);
        case FS_DEV:
            return devfs_read(file, buf, size);
        case FS_FAT16:
            return fat16_read(&file->f.fat16, buf, size);
        case FS_FAT32:
            return fat32_read(&file->f.fat32, buf, size);
        case FS_ISO9660:
            return iso9660_read(&file->f.iso9660, buf, size);
        case FS_EXT2:
            return ext2_read(&file->f.ext2, buf, size);
        default:
            printf("not implemented");
            break;
    }

    return -3;
}

int vfs_write(vfs_file_t *file, const uint8_t *buf, uint32_t size) {
    if (!file || !buf)
        return -1;

    if (!(file->flags & VFS_WRONLY) &&
        !(file->flags & VFS_RDWR)) {
        eprintf("write: file not opened for writing");
        return -2;
    }

    switch (file->mnt->type) {
        case FS_PROC:
            return -10; // not implemented
        case FS_DEV:
            return devfs_write(file, buf, size);
        case FS_FAT16:
            return fat16_write(&file->f.fat16, buf, size);
        case FS_FAT32:
            return fat32_write(&file->f.fat32, buf, size);
        case FS_ISO9660:
            return -10; // read-only
        case FS_EXT2:
            return ext2_write(&file->f.ext2, buf, size);

        default:
            printf("not implemented");
            break;
    }

    return -3;
}

void vfs_close(vfs_file_t *file) {
    if (!file || !file->mnt) {
        eprintf("close: invalid file pointer");
        return;
    }

    switch (file->mnt->type) {
        case FS_PROC:
            return; // not implemented
        case FS_DEV:
            return devfs_close(file);
        case FS_FAT16:
            return fat16_close(&file->f.fat16);
        case FS_FAT32:
            return fat32_close(&file->f.fat32);
        case FS_ISO9660:
            return iso9660_close(&file->f.iso9660);
        case FS_EXT2:
            return ext2_close(&file->f.ext2);

        default:
            printf("not implemented");
            break;
    }
}

int vfs_path_is_dir(const char *path) {
    if (!path || !*path) {
        eprintf("path_is_dir: path is null or undefined");
        return -1;
    }

    char norm[256];
    if (vfs_normalize_path(path, norm, sizeof(norm)) != 0)
        return -1;

    /* Root always exists */
    if (strcmp(norm, "/") == 0)
        return 1;

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -2;

    /* If rel_path is empty, this path IS a mount point itself -> it's a dir */
    if (*res.rel_path == '\0')
        return 1;

    switch (res.mnt->type) {
        case FS_FAT16: {
            fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;
            fat16_dir_entry_t e;
            if (fat16_find_path(fs, res.rel_path, &e) != 0)
                return 0;
            return (e.attr & 0x10) ? 1 : 0;
        }

        case FS_FAT32: {
            fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
            fat32_dir_entry_t e;
            if (fat32_find_path(fs, res.rel_path, &e) != FAT_OK)
                return 0;
            return (e.attr & 0x10) ? 1 : 0;
        }

        case FS_ISO9660: {
            iso9660_fs_t *fs = (iso9660_fs_t *)res.mnt->fs;
            iso9660_dirent_t e;
            if (iso9660_find_path(fs, res.rel_path, &e) != 0)
                return 0;
            return (e.flags & ISO9660_FLAG_DIR) ? 1 : 0;
        }

        case FS_EXT2: {
            ext2_fs_t *fs = (ext2_fs_t *)res.mnt->fs;
            uint32_t ino;
            ext2_inode_t inode;
            if (ext2_find_path(fs, res.rel_path, &ino, &inode) != EXT2_OK)
                return 0;
            return ((inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR) ? 1 : 0;
        }

        case FS_PROC:
        case FS_DEV:
            /* Layered filesystems: treat root as existing, else unsupported */
            return 0;

        default:
            return 0;
    }
}

int vfs_ls(const char *path) {
    if (!path) {
        eprintf("ls: invalid path");
        return -1;
    }

    char norm[256];
    if (vfs_normalize_path(path, norm, sizeof(norm)) != 0)
        return -1;

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -1;

    bool entries = false;

    // for (int i = 1; i < mounted_partition_count; i++) { // i = 1; to ignore the root mount
    //     mount_entry_t* m = &mounted_partitions[i];

    //     if (vfs_is_direct_child_mount(norm, m)) {
    //         const char* name = vfs_basename(m->mount_point);
    //         printfnoln(yellow_color "%s/ " reset_color, name);
    //         entries = true;
    //     }
    // }

    if (res.mnt->type == FS_PROC) {
        procfs_ls(res.rel_path);
        entries = true;
    }
    if (res.mnt->type == FS_DEV) {
        if (res.rel_path[0] == '\0') {
            devfs_ls();
            entries = true;
        }
    }

    if (res.mnt->type == FS_FAT16) {
        fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;

        if (*res.rel_path == '\0') {
            if (fat16_list_root(fs) != 0)
                entries = true;

        } else {
            fat16_dir_entry_t e;
            if (fat16_find_path(fs, res.rel_path, &e) != 0) {
                printf("ls: path not found");
                return -3;
            }

            if (!(e.attr & 0x10)) {
                printf("ls: not a directory");
                return -4;
            }

            if (fat16_list_dir_cluster(fs, e.first_cluster) != 0)
                entries = true;
        }
    }

    if (res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;

        if (*res.rel_path == '\0') {
            fat32_list_root(fs);
        } else {
            fat32_dir_entry_t e;
            if (fat32_find_path(fs, res.rel_path, &e) != FAT_OK) {
                printf("ls: path not found");
                return -3;
            }

            if (!(e.attr & 0x10)) {
                printf("ls: not a directory");
                return -4;
            }

            uint32_t first_cluster = (e.first_cluster_high << 16) | e.first_cluster_low;
            fat32_list_dir_cluster(fs, first_cluster);
        }
    }

    if (res.mnt->type == FS_ISO9660) {
        iso9660_fs_t *fs = (iso9660_fs_t *)res.mnt->fs;

        if (*res.rel_path == '\0') {
            if (iso9660_list_root(fs) == 0)
                entries = true;
        } else {
            iso9660_dirent_t e;
            if (iso9660_find_path(fs, res.rel_path, &e) != 0) {
                printf("ls: path not found");
                return -3;
            }

            if (!(e.flags & ISO9660_FLAG_DIR)) {
                printf("ls: not a directory");
                return -4;
            }

            if (iso9660_list_dir(fs, &e) == 0)
                entries = true;
        }
    }

    if (res.mnt->type == FS_EXT2) {
        ext2_fs_t *fs = (ext2_fs_t *)res.mnt->fs;
        uint32_t dir_ino;

        if (*res.rel_path == '\0') {
            dir_ino = EXT2_ROOT_INO;
        } else {
            if (ext2_find_path(fs, res.rel_path, &dir_ino, NULL) != EXT2_OK) {
                printf("ls: path not found");
                return -3;
            }
        }

        if (ext2_list_dir(fs, dir_ino) == EXT2_OK)
            entries = true;
    }

    if (entries)
        print("\n");
    return 0;
}

int vfs_open(const char *path, int flags, vfs_file_t *out) {
    if (!path || !out) {
        eprintf("open: invalid parameters");
        return -1;
    }

    memset(out, 0, sizeof(*out));

    char norm[256];
    if (vfs_normalize_path(path, norm, sizeof(norm)) != 0)
        return -1;

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -2;

    if (res.mnt->type == FS_PROC) {
        strncpy(out->rel_path, res.rel_path, sizeof(out->rel_path));
        out->mnt = res.mnt;
        out->flags = flags;

        if (res.rel_path[0] == '\0') {
            // Opening /proc itself as a directory — nothing to open,
            // getdents64 handles listing it directly.
            return 0;
        }

        if (procfs_open(out) != 0)
            return -1;

        return 0;
    }

    if (res.mnt->type == FS_DEV) {
        strncpy(out->rel_path, res.rel_path, sizeof(out->rel_path));
        out->mnt = res.mnt;
        out->flags = flags;

        if (res.rel_path[0] == '\0') {
            return 0;
        }

        if (devfs_open(out) != 0)
            return -1;

        return 0;
    }

    if (res.mnt->type == FS_FAT16) {
        fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;
        int ret;

        /* ---------- CREATE ---------- */
        if (flags & VFS_CREATE) {
            /* create if missing */
            ret = fat16_open(fs, res.rel_path, &out->f.fat16);
            if (ret != 0) {
                /* create new file */
                ret = fat16_create_path(fs, res.rel_path,
                    FAT16_ROOT_CLUSTER,
                    0x20); /* archive */
                if (ret != 0)
                    return -4;

                ret = fat16_open(fs, res.rel_path, &out->f.fat16);
                if (ret != 0)
                    return -5;
            }
        } else {
            ret = fat16_open(fs, res.rel_path, &out->f.fat16);
            if (ret != 0)
                return -6;
        }

        /* ---------- TRUNC ---------- */
        if (flags & VFS_TRUNC) {
            fat16_truncate(&out->f.fat16, 0);
        }

        /* ---------- APPEND ---------- */
        if (flags & VFS_APPEND) {
            out->f.fat16.pos = out->f.fat16.entry.filesize;
        }

        out->mnt = res.mnt;
        out->flags = flags;
        if (flags & VFS_APPEND) {
            out->f.fat16.pos = out->f.fat16.entry.filesize;
        } else {
            out->f.fat16.pos = 0;
        }

        return 0;
    } else if (res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
        int ret;

        /* ---------- CREATE ---------- */
        if (flags & VFS_CREATE) {
            ret = fat32_open(fs, res.rel_path, &out->f.fat32);
            if (ret != 0) {
                ret = fat32_create_path(fs, res.rel_path, 0x20); // archive
                if (ret != 0)
                    return -4;

                ret = fat32_open(fs, res.rel_path, &out->f.fat32);
                if (ret != 0)
                    return -5;
            }
        } else {
            ret = fat32_open(fs, res.rel_path, &out->f.fat32);
            if (ret != 0)
                return -6;
        }

        /* ---------- TRUNC ---------- */
        if (flags & VFS_TRUNC) {
            fat32_truncate(&out->f.fat32, 0);
        }

        /* ---------- APPEND ---------- */
        if (flags & VFS_APPEND) {
            out->f.fat32.pos = out->f.fat32.entry.file_size;
        }

        out->mnt = res.mnt;
        out->flags = flags;

        if (flags & VFS_APPEND) {
            out->f.fat32.pos = out->f.fat32.entry.file_size;
        } else {
            out->f.fat32.pos = 0;
        }
        return 0;
    }

    if (res.mnt->type == FS_ISO9660) {
        if (flags & (VFS_CREATE | VFS_TRUNC | VFS_APPEND | VFS_WRONLY | VFS_RDWR)) {
            eprintf("open: iso9660 is read-only");
            return -7;
        }

        iso9660_fs_t *fs = (iso9660_fs_t *)res.mnt->fs;
        int ret = iso9660_open(fs, res.rel_path, &out->f.iso9660);
        if (ret != 0)
            return -6;

        out->mnt = res.mnt;
        out->flags = flags;
        return 0;
    }

    if (res.mnt->type == FS_EXT2) {
        ext2_fs_t *fs = (ext2_fs_t *)res.mnt->fs;
        int ret;

        /* ---------- CREATE ---------- */
        if (flags & VFS_CREATE) {
            ret = ext2_open(fs, res.rel_path, &out->f.ext2);
            if (ret != EXT2_OK) {
                ret = ext2_create(fs, res.rel_path, 0644, &out->f.ext2);
                if (ret != EXT2_OK)
                    return -4;
            }
        } else {
            ret = ext2_open(fs, res.rel_path, &out->f.ext2);
            if (ret != EXT2_OK)
                return -6;
        }

        if (out->f.ext2.is_dir && (flags & (VFS_WRONLY | VFS_TRUNC))) {
            eprintf("open: is a directory");
            return -7;
        }

        /* ---------- TRUNC ---------- */
        if (flags & VFS_TRUNC) {
            ext2_truncate(fs, &out->f.ext2);
        }

        out->mnt = res.mnt;
        out->flags = flags;

        /* ---------- APPEND ---------- */
        if (flags & VFS_APPEND)
            out->f.ext2.pos = out->f.ext2.inode.i_size;
        else
            out->f.ext2.pos = 0;

        return 0;
    }

    eprintf("open: unknown filesystem");
    return -3;
}

int vfs_mkdir(const char *path) {
    if (!path) {
        eprintf("mkdir: path is null or undefined");
        return -1;
    }

    char norm[256];
    if (vfs_normalize_path(path, norm, sizeof(norm)) != 0)
        return -1;

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -1;

    if (res.mnt->type == FS_FAT16) {
        fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;
        uint16_t parent_cluster = FAT16_ROOT_CLUSTER;
        return fat16_create_path(fs, res.rel_path, parent_cluster, 0x10);
    }

    if (res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
        return fat32_create_path(fs, res.rel_path, 0x10);
    }

    if (res.mnt->type == FS_EXT2) {
        ext2_fs_t *fs = (ext2_fs_t *)res.mnt->fs;
        return ext2_mkdir(fs, res.rel_path);
    }

    printf("mkdir: unknown filesystem");
    return -2;
}

int vfs_rm_recursive(const char *path) {
    char norm[256];
    if (vfs_normalize_path(path, norm, sizeof(norm)) != 0)
        return -1;

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -1;

    if (res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
        return fat32_rm_recursive(fs, res.rel_path);
    }

    if (res.mnt->type == FS_EXT2) {
        ext2_fs_t *fs = (ext2_fs_t *)res.mnt->fs;
        return ext2_rm_recursive(fs, res.rel_path);
    }

    if (res.mnt->type != FS_FAT16)
        return -1;

    fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;

    uint16_t parent = FAT16_ROOT_CLUSTER;
    char name[13] = {0};

    /* split parent + name */
    char tmp[256];
    strncpy(tmp, res.rel_path, sizeof(tmp));
    tmp[sizeof(tmp) - 1] = 0;

    char *slash = strrchr(tmp, '/');
    if (slash) {
        *slash = 0;
        strncpy(name, slash + 1, sizeof(name));
        name[sizeof(name) - 1] = 0;

        if (*tmp) {
            fat16_dir_entry_t p;
            if (fat16_find_path(fs, tmp, &p) != 0)
                return -1;
            parent = p.first_cluster;
        }
    } else {
        strncpy(name, tmp, sizeof(name));
        name[sizeof(name) - 1] = 0;
    }

    /* NEVER allow these */
    if (!strcmp(name, ".") || !strcmp(name, ".."))
        return -1;

    fat16_dir_entry_t e;
    if (fat16_find_in_dir(fs, parent, name, &e) != 0)
        return -1;

    if (e.attr & 0x10) {
        fat16_rmdir(fs, e.first_cluster);
    } else {
        fat16_free_chain(fs, e.first_cluster);
    }

    fat16_delete_entry(fs, parent, name);
    return 0;
}

int vfs_cd(const char *path) {
    if (!path || !*path) {
        eprintf("cd: path is null or undefined");
        return -1;
    }

    char norm[256];
    if (vfs_normalize_path(path, norm, sizeof(norm)) != 0)
        return -1;

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -1;

    /* ---------- PROCFS ---------- */
    if (res.mnt->type == FS_PROC) {
        vfs_cwd_cluster = 0;
        strncpy(vfs_cwd, norm, sizeof(vfs_cwd));
        vfs_cwd[sizeof(vfs_cwd) - 1] = 0;
        return 0;
    }

    if (res.mnt->type == FS_DEV) {
        if (*res.rel_path)
            return -4;

        vfs_cwd_cluster = 0;
        strncpy(vfs_cwd, norm, sizeof(vfs_cwd));
        vfs_cwd[sizeof(vfs_cwd) - 1] = 0;
        return 0;
    }

    if (res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
        uint32_t new_cluster = fs->root_cluster;

        if (*res.rel_path) {
            fat32_dir_entry_t e;
            if (fat32_find_path(fs, res.rel_path, &e) != FAT_OK)
                return -3;

            if (!(e.attr & 0x10))
                return -4;

            new_cluster = (e.first_cluster_high << 16) | e.first_cluster_low;
        }

        vfs_cwd_cluster = new_cluster;
        strncpy(vfs_cwd, norm, sizeof(vfs_cwd));
        return 0;
    }

    if (res.mnt->type == FS_ISO9660) {
        iso9660_fs_t *fs = (iso9660_fs_t *)res.mnt->fs;

        if (*res.rel_path) {
            iso9660_dirent_t e;
            if (iso9660_find_path(fs, res.rel_path, &e) != 0)
                return -3;

            if (!(e.flags & ISO9660_FLAG_DIR))
                return -4;
        }

        vfs_cwd_cluster = 0;
        strncpy(vfs_cwd, norm, sizeof(vfs_cwd));
        vfs_cwd[sizeof(vfs_cwd) - 1] = 0;
        return 0;
    }

    if (res.mnt->type == FS_EXT2) {
        ext2_fs_t *fs = (ext2_fs_t *)res.mnt->fs;
        uint32_t new_ino = EXT2_ROOT_INO;

        if (*res.rel_path) {
            ext2_inode_t e;
            if (ext2_find_path(fs, res.rel_path, &new_ino, &e) != EXT2_OK)
                return -3;
            if ((e.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
                return -4;
        }

        fs->cwd_ino = new_ino;
        strncpy(vfs_cwd, norm, sizeof(vfs_cwd));
        vfs_cwd[sizeof(vfs_cwd) - 1] = 0;
        return 0;
    }

    /* ---------- FAT16 ---------- */
    if (res.mnt->type != FS_FAT16) {
        eprintf("cd: unknown filesystem");
        return -2;
    }

    fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;
    uint16_t new_cluster = FAT16_ROOT_CLUSTER;

    if (*res.rel_path) {
        fat16_dir_entry_t e;
        if (fat16_find_path(fs, res.rel_path, &e) != FAT_OK)
            return -3;

        if (!(e.attr & 0x10))
            return -4;

        new_cluster = e.first_cluster;
    }

    vfs_cwd_cluster = new_cluster;
    strncpy(vfs_cwd, norm, sizeof(vfs_cwd));
    vfs_cwd[sizeof(vfs_cwd) - 1] = 0;
    return 0;
}

int vfs_create_path(const char *path, uint8_t attr) {
    if (!path || !*path) {
        eprintf("create_path: path is null or undefined");
        return -1;
    }

    char norm[256];
    if (vfs_normalize_path(path, norm, sizeof(norm)) != 0)
        return -1;

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -1;
    if (res.mnt->type == FS_FAT16) {
        fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;
        uint16_t parent_cluster = FAT16_ROOT_CLUSTER; // root
        return fat16_create_path(fs, res.rel_path, parent_cluster, attr);
    }

    if (res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
        return fat32_create_path(fs, res.rel_path, attr);
    }

    if (res.mnt->type == FS_EXT2) {
        ext2_fs_t *fs = (ext2_fs_t *)res.mnt->fs;

        /* Mirror FAT convention: bit 0x10 means "this is a directory". */
        if (attr & 0x10)
            return ext2_mkdir(fs, res.rel_path);

        return ext2_create(fs, res.rel_path, 0644, NULL);
    }

    return -2;
}

int vfs_unlink(const char *path) {
    if (!path || !*path) {
        eprintf("unlink:: path is null or undefined");
        return -1;
    }
    /* Normalize */
    char norm[256];
    if (vfs_normalize_path(path, norm, sizeof(norm)) != 0)
        return -1;

    /* Resolve mount */
    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -2;

    if (res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)res.mnt->fs;
        return fat32_unlink_path(fs, res.rel_path);
    }

    if (res.mnt->type == FS_EXT2) {
        ext2_fs_t *fs = (ext2_fs_t *)res.mnt->fs;
        return ext2_unlink_path(fs, res.rel_path);
    }

    if (res.mnt->type != FS_FAT16)
        return -3;

    fat16_fs_t *fs = (fat16_fs_t *)res.mnt->fs;

    /* Split parent + name (RELATIVE PATH, NO LEADING '/') */
    char parent_path[256];
    char name[13];

    const char *last = strrchr(res.rel_path, '/');

    if (last) {
        /* Has parent directories */
        size_t plen = last - res.rel_path;
        memcpy(parent_path, res.rel_path, plen);
        parent_path[plen] = 0;
        strncpy(name, last + 1, sizeof(name));
    } else {
        /* Direct child of root */
        parent_path[0] = 0;
        strncpy(name, res.rel_path, sizeof(name));
    }

    name[sizeof(name) - 1] = 0;

    /* Resolve parent cluster */
    uint16_t parent_cluster = FAT16_ROOT_CLUSTER;

    if (parent_path[0]) {
        fat16_dir_entry_t parent;
        if (fat16_find_path(fs, parent_path, &parent) != 0)
            return -4;

        if (!(parent.attr & 0x10))
            return -5;

        parent_cluster = parent.first_cluster;
    }

    /* Delete */
    return fat16_unlink_path(fs, parent_cluster, name);
}

int vfs_mv(const char *src, const char *dst) {
    char src_norm[256], dst_norm[256];
    if (vfs_normalize_path(src, src_norm, sizeof(src_norm)) != 0)
        return -1;
    if (vfs_normalize_path(dst, dst_norm, sizeof(dst_norm)) != 0)
        return -1;

    /* If destination is an existing directory, move INTO it under the
     * source's own basename (same convention as vfs_cp / standard 'mv'). */
    if (vfs_path_is_dir(dst_norm) == 1) {
        const char *base = vfs_basename(src_norm);
        char combined[256];
        size_t dlen = strlen(dst_norm);

        if (dlen > 0 && dst_norm[dlen - 1] == '/')
            snprintf(combined, sizeof(combined), "%s%s", dst_norm, base);
        else
            snprintf(combined, sizeof(combined), "%s/%s", dst_norm, base);

        if (vfs_normalize_path(combined, dst_norm, sizeof(dst_norm)) != 0)
            return -1;
    }

    vfs_mount_res_t src_res, dst_res;
    if (vfs_resolve_mount(src_norm, &src_res) != 0)
        return -1;
    if (vfs_resolve_mount(dst_norm, &dst_res) != 0)
        return -1;

    /* ---------- CROSS-DEVICE MOVE ---------- */
    if (src_res.mnt != dst_res.mnt) {
        int src_is_dir = vfs_path_is_dir(src_norm);
        if (src_is_dir < 0) {
            eprintf("mv: source path does not resolve: %s", src_norm);
            return -1;
        }
        if (src_is_dir == 1) {
            /* Would need recursive cross-fs copy, which needs a generic
             * readdir() the VFS doesn't expose yet. */
            eprintf("mv: cross-device move of directories is not supported");
            return -1;
        }

        int ret = vfs_cp(src_norm, dst_norm);
        if (ret != 0) {
            eprintf("mv: cross-device copy failed (%d)", ret);
            return ret;
        }

        if (vfs_unlink(src_norm) != 0) {
            eprintf("mv: copied '%s' -> '%s' but failed to remove source",
                     src_norm, dst_norm);
            return -10;
        }

        printf("mv: moved '%s' -> '%s' (cross-device)", src_norm, dst_norm);
        return 0;
    }

    /* ---------- SAME-DEVICE MOVE (fast path) ---------- */

    if (src_res.mnt->type == FS_FAT32) {
        fat32_fs_t *fs = (fat32_fs_t *)src_res.mnt->fs;
        return fat32_mv(fs, src_res.rel_path, dst_res.rel_path);
    }

    if (src_res.mnt->type == FS_EXT2) {
        ext2_fs_t *fs = (ext2_fs_t *)src_res.mnt->fs;
        return ext2_rename(fs, src_res.rel_path, dst_res.rel_path);
    }

    if (src_res.mnt->type != FS_FAT16)
        return -1;

    fat16_fs_t *fs = (fat16_fs_t *)src_res.mnt->fs;

    /* ---------- SPLIT SRC ---------- */
    uint16_t src_parent = FAT16_ROOT_CLUSTER;
    char src_name[13];

    char src_tmp[256];
    strncpy(src_tmp, src_res.rel_path, sizeof(src_tmp));
    src_tmp[sizeof(src_tmp) - 1] = 0;

    char *s = strrchr(src_tmp, '/');
    if (s) {
        *s = 0;
        strncpy(src_name, s + 1, sizeof(src_name));
        src_name[sizeof(src_name) - 1] = 0;

        if (*src_tmp) {
            fat16_dir_entry_t e;
            if (fat16_find_path(fs, src_tmp, &e) != 0)
                return -1;
            src_parent = e.first_cluster;
        }
    } else {
        strncpy(src_name, src_tmp, sizeof(src_name));
        src_name[sizeof(src_name) - 1] = 0;
    }

    /* ---------- SPLIT DST ---------- */
    uint16_t dst_parent = FAT16_ROOT_CLUSTER;
    char dst_name[13];

    char dst_tmp[256];
    strncpy(dst_tmp, dst_res.rel_path, sizeof(dst_tmp));
    dst_tmp[sizeof(dst_tmp) - 1] = 0;

    char *d = strrchr(dst_tmp, '/');
    if (d) {
        *d = 0;
        strncpy(dst_name, d + 1, sizeof(dst_name));
        dst_name[sizeof(dst_name) - 1] = 0;

        if (*dst_tmp) {
            fat16_dir_entry_t e;
            if (fat16_find_path(fs, dst_tmp, &e) != 0)
                return -1;
            dst_parent = e.first_cluster;
        }
    } else {
        strncpy(dst_name, dst_tmp, sizeof(dst_name));
        dst_name[sizeof(dst_name) - 1] = 0;
    }

    /* ---------- REAL MOVE ---------- */
    return fat16_mv(fs, src_parent, src_name, dst_parent, dst_name);
}

const char *vfs_getcwd(void) {
    return vfs_cwd;
}

int vfs_is_direct_child_mount(const char *parent, mount_entry_t *m) {
    if (!strcmp(parent, "/")) {
        if (m->mount_point[0] != '/')
            return 0;
        /* Only one slash allowed */
        const char *rest = m->mount_point + 1;
        return strchr(rest, '/') == NULL;
    }

    size_t plen = strlen(parent);
    if (strncmp(m->mount_point, parent, plen) != 0)
        return 0;

    if (m->mount_point[plen] != '/')
        return 0;

    return strchr(m->mount_point + plen + 1, '/') == NULL;
}

const char *vfs_basename(const char *path) {
    const char *last = path;
    while (*path) {
        if (*path == '/')
            last = path + 1;
        path++;
    }
    return last;
}

int vfs_sync(bool kernel_call) {
    int ret = 0;

    for (int i = 0; i < mounted_partition_count; i++) {
        mount_entry_t *mnt = &mounted_partitions[i];

        switch (mnt->type) {

            case FS_FAT16:
                ret |= fat16_sync((fat16_fs_t *)mnt->fs);
                break;

            case FS_FAT32:
                ret |= fat32_sync((fat32_fs_t *)mnt->fs);
                break;

            case FS_EXT2:
                ret |= ext2_sync((ext2_fs_t *)mnt->fs);
                break;

            case FS_ISO9660:
            case FS_PROC:
            case FS_DEV:
                break;
        }
    }

    klog_printf("Sync has been called!");
    if(kernel_call){
        LOG_SCOPE();
        info("Syncing all disks", __FILE__);
    }
    return ret;
}

int vfs_exec(const char *path, int argc, const char **argv) {
    return userland_exec(path, argc, argv, NULL);
}

const char *fs_type_to_string(int fs) {
    switch (fs) {
        case FS_FAT16:
            return "FAT16";
        case FS_FAT32:
            return "FAT32";
        case FS_PROC:
            return "PROCFS";
        case FS_DEV:
            return "DEVFS";
        case FS_ISO9660:
            return "ISO9660";
        case FS_EXT2:
            return "EXT2";
        default:
            return "UNKNOWN";
    }
}


int vfs_mount(const char *diskname, const char *mount_point, bool is_kernel_call) {
    // Strip leading "/dev/" if present
    const char *device = diskname;
    if (strncmp(device, "/dev/", 5) == 0) {
        device += 5;
    }

    if (strcmp(mount_point, "/") != 0) {
        vfs_mount_res_t res;
        if (vfs_resolve_mount("/", &res) != 0) {
            if (is_kernel_call)
                error("mount: cannot mount block device, root (/) is not mounted.", __FILE__, device);
            else
                eprintf("mount: cannot mount block device, root (/) is not mounted.");
            return -2;
        }

        int is_dir = vfs_path_is_dir(mount_point);
        if (is_dir <= 0) {
            if (is_kernel_call)
                error("mount: mount point '%s' does not exist on the underlying filesystem.", __FILE__, mount_point);
            else
                eprintf("mount: mount point '%s' does not exist on the underlying filesystem.", mount_point);
            return -3;
        }
    }

    if (strcmp(device, "proc") == 0) {
        mount_entry_t *new_mount = add_mount(mount_point, device, FS_PROC, NULL);
        if (!new_mount) {
            if (is_kernel_call)
                error("mount: failed to add mount entry for %s.", __FILE__, device);
            return 1;
        }

        procfs_init();

        if (strcmp(mount_point, "/proc") != 0) {
            if (is_kernel_call)
                warn("mount: warning mounting 'proc' on non-standard path.", __FILE__, mount_point);
            else
                printf("mount: warning mounting \'proc\' on non-standard path.");
        }

        if (is_kernel_call)
            done("mount: mounted %s (%s) at '%s'.", __FILE__, device, fs_type_to_string(FS_PROC), mount_point);
        else
            printf("mount: mounted " red_color "%s" reset_color " (%s) at '%s'",
                device,
                fs_type_to_string(FS_PROC),
                mount_point);

        return 0;
    }

    if (strcmp(device, "dev") == 0) {
        mount_entry_t *new_mount = add_mount(mount_point, device, FS_DEV, NULL);
        if (!new_mount) {
            if (is_kernel_call)
                error("mount: failed to add mount entry for %s.", __FILE__, device);
            return 1;
        }

        devfs_init();

        if (strcmp(mount_point, "/dev") != 0) {
            if (is_kernel_call)
                warn("mount: warning mounting 'dev' on non-standard path.", __FILE__, mount_point);
            else
                printf("mount: warning mounting 'dev' on non-standard path.");
        }

        if (is_kernel_call)
            done("mount: mounted %s (%s) at '%s'.", __FILE__, device, fs_type_to_string(FS_DEV), mount_point);
        else
            printf("mount: mounted " red_color "%s" reset_color " (%s) at '%s'",
                device,
                fs_type_to_string(FS_DEV),
                mount_point);

        return 0;
    }

    general_partition_t *partition = search_general_partition(device);
    int raw_device_id = -1;

    if (!partition) {
        for (int i = 0; i < block_device_count; i++) {
            if (!block_devices[i].present)
                continue;
            if (strcmp(block_devices[i].name, device) == 0) {
                raw_device_id = i;
                break;
            }
        }

        if (raw_device_id < 0) {
            if (is_kernel_call)
                error("mount: %s: partition not found.", __FILE__, device);
            else
                printf("mount: %s: partition not found.", device);
            return 1;
        }
    }

    void *fs_struct = NULL;
    int ret = 0;

    partition_fs_type_t mount_fs = partition ? partition->fs_type : FS_ISO9660;

    switch (mount_fs) {
        case FS_FAT16:
            fs_struct = kmalloc(sizeof(fat16_fs_t));
            if (!fs_struct) {
                if (is_kernel_call)
                    error("mount: memory allocation failed.", __FILE__, device);
                else
                    printf("mount: memory allocation failed.");
                return 1;
            }
            mount_entry_t *mount1 = add_mount(mount_point, device, partition->fs_type, fs_struct);
            if (!mount1) {
                if (is_kernel_call)
                    error("mount: failed to add mount entry for %s.", __FILE__, device);
                return 1;
            }

            ret = fat16_mount(partition->ahci_port, partition->lba_start, (fat16_fs_t *)fs_struct);
            break;

        case FS_FAT32:
            fs_struct = kmalloc(sizeof(fat32_fs_t));
            if (!fs_struct) {
                if (is_kernel_call)
                    error("mount: memory allocation failed.", __FILE__, device);
                else
                    printf("mount: memory allocation failed.");
                return 1;
            }
            mount_entry_t *mount2 = add_mount(mount_point, device, partition->fs_type, fs_struct);
            if (!mount2) {
                if (is_kernel_call)
                    error("mount: failed to add mount entry for %s.", __FILE__, device);
                return 1;
            }

            ret = fat32_mount(partition->ahci_port, partition->lba_start, (fat32_fs_t *)fs_struct);
            break;

        case FS_ISO9660:
            fs_struct = kmalloc(sizeof(iso9660_fs_t));
            if (!fs_struct) {
                if (is_kernel_call)
                    error("mount: memory allocation failed.", __FILE__, device);
                else
                    printf("mount: memory allocation failed.");
                return 1;
            }
            mount_entry_t *mount3 = add_mount(mount_point, device, mount_fs, fs_struct);
            if (!mount3) {
                if (is_kernel_call)
                    error("mount: failed to add mount entry for %s.", __FILE__, device);
                return 1;
            }

            if (partition)
                ret = iso9660_mount(partition->ahci_port, partition->lba_start, (iso9660_fs_t *)fs_struct);
            else
                ret = iso9660_mount(raw_device_id, 0, (iso9660_fs_t *)fs_struct);
            break;

        case FS_EXT2:
            fs_struct = kmalloc(sizeof(ext2_fs_t));
            if (!fs_struct) {
                if (is_kernel_call)
                    error("mount: memory allocation failed.", __FILE__, device);
                else
                    printf("mount: memory allocation failed.");
                return 1;
            }
            mount_entry_t *mount4 = add_mount(mount_point, device, partition->fs_type, fs_struct);
            if (!mount4) {
                if (is_kernel_call)
                    error("mount: failed to add mount entry for %s.", __FILE__, device);
                return 1;
            }

            ret = ext2_mount(partition->ahci_port, partition->lba_start, (ext2_fs_t *)fs_struct);
            break;

        default:
            if (is_kernel_call)
                error("mount: unsupported filesystem.", __FILE__, device);
            else
                printf("mount: unsupported filesystem.");
            return 1;
    }

    if (ret != 0) {
        if (is_kernel_call)
            error("mount: mounting of %s failed.", __FILE__, device);
        else
            printf("mount: mounting failed.");
        return 1;
    }

    if (is_kernel_call)
        done("mount: mounted %s (%s) at '%s'.", __FILE__, device, fs_type_to_string(mount_fs), mount_point);
    else
        printf("mount: mounted %s (%s) at '%s'",
            device,
            fs_type_to_string(mount_fs),
            mount_point);

    return 0;
}

int vfs_umount(const char *mount_point, bool is_kernel_call) {
    /* Never allow root unmount */
    if (strcmp(mount_point, "/") == 0) {
        if (is_kernel_call)
            warn("umount: unmounting root filesystem.", __FILE__, mount_point);
        else
            printf("umount: warn unmounting root filesystem");
    }

    mount_entry_t *m = find_mount_by_point(mount_point);
    if (!m) {
        if (is_kernel_call)
            error("umount: %s: not mounted.", __FILE__, mount_point);
        else
            printf("umount: %s: not mounted", mount_point);
        return 1;
    }

    /* Filesystem-specific cleanup */
    switch (m->type) {
        case FS_FAT16:
            if (m->fs) {
                fat16_unmount((fat16_fs_t *)m->fs);
                kfree(m->fs);
            }
            break;
        case FS_EXT2:
            if (m->fs) {
                ext2_unmount((ext2_fs_t *)m->fs);
                kfree(m->fs);
            }
            break;
        case FS_PROC:
            // procfs_shutdown(); /* or procfs_unmount() */
            break;
        case FS_DEV:
            break;
        default:
            if (is_kernel_call)
                error("umount: unsupported filesystem.", __FILE__, mount_point);
            else
                printf("umount: unsupported filesystem");
            return 1;
    }

    if (remove_mount(mount_point) != 0) {
        if (is_kernel_call)
            error("umount: failed to remove mount for %s.", __FILE__, mount_point);
        else
            printf("umount: failed to remove mount");
        return 1;
    }

    if (is_kernel_call)
        done("umount: %s unmounted.", __FILE__, mount_point);
    else
        printf("umount: %s unmounted", mount_point);

    return 0;
}

int vfs_umount_all(bool is_kernel_call) {
    int ret = 0;

    /* Unmount every non-root mount first. We restart the scan after each
     * removal since remove_mount() shifts the mounted_partitions array,
     * and we copy the mount_point out before calling vfs_umount() in case
     * the entry's backing memory moves/gets invalidated during removal. */
    bool progress = true;
    while (progress) {
        progress = false;

        for (int i = 0; i < mounted_partition_count; i++) {
            mount_entry_t *m = &mounted_partitions[i];

            if (strcmp(m->mount_point, "/") == 0)
                continue;

            char mount_point_copy[256];
            strncpy(mount_point_copy, m->mount_point, sizeof(mount_point_copy) - 1);
            mount_point_copy[sizeof(mount_point_copy) - 1] = '\0';

            if (vfs_umount(mount_point_copy, is_kernel_call) != 0)
                ret = 1;

            progress = true;
            break; /* array shifted, restart the scan */
        }
    }

    /* Finally unmount root */
    for (int i = 0; i < mounted_partition_count; i++) {
        mount_entry_t *m = &mounted_partitions[i];

        if (strcmp(m->mount_point, "/") == 0) {
            char mount_point_copy[256];
            strncpy(mount_point_copy, m->mount_point, sizeof(mount_point_copy) - 1);
            mount_point_copy[sizeof(mount_point_copy) - 1] = '\0';

            if (vfs_umount(mount_point_copy, is_kernel_call) != 0)
                ret = 1;

            break;
        }
    }

    klog_printf("Unmount all has been called!");
    if (is_kernel_call) {
        LOG_SCOPE();
        info("Unmounting all disks", __FILE__);
    }

    return ret;
}

#define VFS_CP_BUF_SIZE 4096

int vfs_cp(const char *src, const char *dst) {
    if (!src || !dst || !*src || !*dst) {
        eprintf("cp: invalid arguments");
        return -1;
    }

    char src_norm[256];
    if (vfs_normalize_path(src, src_norm, sizeof(src_norm)) != 0)
        return -1;

    int src_is_dir = vfs_path_is_dir(src_norm);
    if (src_is_dir < 0) {
        eprintf("cp: source path does not resolve: %s", src_norm);
        return -2;
    }
    if (src_is_dir == 1) {
        /* No generic readdir() abstraction exists in the VFS yet, so
         * recursive directory copy isn't supported here. */
        eprintf("cp: recursive directory copy not supported");
        return -3;
    }

    char dst_norm[256];
    if (vfs_normalize_path(dst, dst_norm, sizeof(dst_norm)) != 0)
        return -1;

    /* If destination is an existing directory, copy INTO it under the
     * source's own basename (standard 'cp' behaviour). */
    if (vfs_path_is_dir(dst_norm) == 1) {
        const char *base = vfs_basename(src_norm);
        char combined[256];
        size_t dlen = strlen(dst_norm);

        if (dlen > 0 && dst_norm[dlen - 1] == '/')
            snprintf(combined, sizeof(combined), "%s%s", dst_norm, base);
        else
            snprintf(combined, sizeof(combined), "%s/%s", dst_norm, base);

        if (vfs_normalize_path(combined, dst_norm, sizeof(dst_norm)) != 0)
            return -1;
    }

    if (!strcmp(src_norm, dst_norm)) {
        eprintf("cp: source and destination are the same file");
        return -4;
    }

    vfs_file_t src_file;
    if (vfs_open(src_norm, VFS_RDONLY, &src_file) != 0) {
        eprintf("cp: cannot open source file: %s", src_norm);
        return -5;
    }

    vfs_file_t dst_file;
    if (vfs_open(dst_norm, VFS_WRONLY | VFS_CREATE | VFS_TRUNC, &dst_file) != 0) {
        eprintf("cp: cannot open/create destination file: %s", dst_norm);
        vfs_close(&src_file);
        return -6;
    }

    uint8_t *buf = (uint8_t *)kmalloc(VFS_CP_BUF_SIZE);
    if (!buf) {
        eprintf("cp: out of memory");
        vfs_close(&src_file);
        vfs_close(&dst_file);
        return -7;
    }

    int ret = 0;
    for (;;) {
        int r = vfs_read(&src_file, buf, VFS_CP_BUF_SIZE);
        if (r < 0) {
            eprintf("cp: read error on %s", src_norm);
            ret = -8;
            break;
        }
        if (r == 0)
            break; /* EOF */

        int w = vfs_write(&dst_file, buf, (uint32_t)r);
        if (w < 0 || (uint32_t)w != (uint32_t)r) {
            eprintf("cp: write error on %s (short write)", dst_norm);
            ret = -9;
            break;
        }

        if ((uint32_t)r < VFS_CP_BUF_SIZE)
            break; /* short read -> EOF */
    }

    kfree(buf);
    vfs_close(&src_file);
    vfs_close(&dst_file);

    if (ret == 0)
        printf("cp: copied '%s' -> '%s'", src_norm, dst_norm);

    return ret;
}