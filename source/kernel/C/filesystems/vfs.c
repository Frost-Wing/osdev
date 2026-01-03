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

#include <filesystems/vfs.h>
#include <basics.h>
#include <graphics.h>
#include <filesystems/fat16.h>
#include <heap.h>
#include <strings.h>

char vfs_cwd[256] = "/";
uint16_t vfs_cwd_cluster = 0; 

static int path_matches_mount(const char* path, const char* mount) {
    if (!path || !mount) return 0;

    /* Root mount matches everything absolute */
    if (strcmp(mount, "/") == 0)
        return path[0] == '/';

    size_t mlen = strlen(mount);
    if (strncmp(path, mount, mlen) != 0)
        return 0;

    return path[mlen] == '\0' || path[mlen] == '/';
}

static int vfs_resolve_mount(const char* path, vfs_mount_res_t* out) {
    if (!path || !out) {
        eprintf("resolve_mount: invalid arguments");
        return -1;
    }

    mount_entry_t* best = NULL;
    int best_len = -1;

    for (int i = 0; i < mounted_partition_count; i++) {
        mount_entry_t* m = &mounted_partitions[i];
        int len = strlen(m->mount_point);

        if (path_matches_mount(path, m->mount_point)) {
            if (len > best_len) {
                best = m;
                best_len = len;
            }
        }
    }

    if (!best) {
        if(path)
            eprintf("resolve_mount: no mount matches path: %s", path);
        else
            eprintf("resolve_mount: no mount matches the given path.");
        return -2;
    }

    const char* rel = path + best_len;
    if (*rel == '/') rel++;

    out->mnt = best;
    out->rel_path = rel;
    return 0;
}

static void vfs_normalize_path(const char* in, char* out) {
    char tmp[256];

    // Start with absolute or relative
    if (in[0] != '/') {
        snprintf(tmp, sizeof(tmp), "%s/%s", vfs_cwd, in);
    } else {
        strncpy(tmp, in, sizeof(tmp));
    }

    int oi = 0;
    const char* p = tmp;

    while (*p) {
        // Skip extra '/'
        while (*p == '/') p++;
        if (!*p) break;

        // Handle '.'
        if (!strncmp(p, ".", 1) && (p[1] == '/' || p[1] == '\0')) {
            p += 1;
            continue;
        }

        // Handle '..'
        if (!strncmp(p, "..", 2) && (p[2] == '/' || p[2] == '\0')) {
            // Remove last directory from 'out'
            if (oi > 1) oi--;           // step back from trailing '/'
            while (oi > 0 && out[oi - 1] != '/') oi--;
            if (oi == 0) oi = 1;        // always keep leading '/'
            p += 2;
            continue;
        }

        // Add '/' before next component
        if (oi == 0 || out[oi - 1] != '/') out[oi++] = '/';

        // Copy next component
        while (*p && *p != '/') out[oi++] = *p++;
    }

    if (oi == 0) out[oi++] = '/';
    out[oi] = '\0';
}

int vfs_read(vfs_file_t* file, uint8_t* buf, uint32_t size) {
    if (!file || !file->mnt || !buf){
        eprintf("read: invalid parameters passed");
        return -1;
    }

    if (file->mnt->type == FS_FAT16)
        return fat16_read(&file->f, buf, size);

    eprintf("read: unknown filesystem");
    return -2;
}

int vfs_write(vfs_file_t* file, const uint8_t* buf, uint32_t size) {
    if (!file || !file->mnt || !buf){
        eprintf("write: invalid parameters passed");
        return -1;
    }

    if (file->mnt->type == FS_FAT16)
        return fat16_write(&file->f, buf, size);

    eprintf("write: unknown filesystem");
    return -2;
}

void vfs_close(vfs_file_t* file) {
    if (!file || !file->mnt) {
        eprintf("close: invalid file pointer");
        return;
    }

    if (file->mnt->type == FS_FAT16)
        fat16_close(&file->f);
}

int vfs_ls(const char* path)
{
    if (!path) {
        eprintf("ls: invalid path");
        return -1;
    }

    char norm[256];
    vfs_normalize_path(path, norm);

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -1;

    bool entries = false;
    
    for (int i = 1; i < mounted_partition_count; i++) { // i = 1; to ignore the root mount
        mount_entry_t* m = &mounted_partitions[i];

        if (vfs_is_direct_child_mount(norm, m)) {
            const char* name = vfs_basename(m->mount_point);
            printfnoln(yellow_color "%s " reset_color, name);
            entries = true;
        }
    }

    if (res.mnt->type == FS_FAT16) {
        fat16_fs_t* fs = (fat16_fs_t*)res.mnt->fs;

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

    if(entries)
        print("\n");
    return 0;
}

int vfs_open(const char* path, vfs_file_t* out) {
    if (!path || !out) {
        eprintf("open: invalid parameters passed");
        return -1;
    }

    char norm[256];
    vfs_normalize_path(path, norm);

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0) return -2;

    if (res.mnt->type == FS_FAT16) {
        if (fat16_open((fat16_fs_t*)res.mnt->fs, res.rel_path, &out->f) != 0)
            return -4;

        out->mnt = res.mnt;

        return 0;
    }

    eprintf("open: unknown filesystem");
    return -3;
}

int vfs_mkdir(const char* path) {
    if (!path){
        eprintf("mkdir: path is null or undefined");
        return -1;
    }

    char norm[256];
    vfs_normalize_path(path, norm);

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0) return -1;

    if (res.mnt->type == FS_FAT16) {
        fat16_fs_t* fs = (fat16_fs_t*)res.mnt->fs;
        uint16_t parent_cluster = FAT16_ROOT_CLUSTER;
        return fat16_create_path(fs, res.rel_path, parent_cluster, 0x10);
    }

    printf("mkdir: unknown filesystem");
    return -2;
}

int vfs_rm_recursive(const char* path)
{
    char norm[256];
    vfs_normalize_path(path, norm);

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -1;

    if (res.mnt->type != FS_FAT16)
        return -1;

    fat16_fs_t* fs = (fat16_fs_t*)res.mnt->fs;

    uint16_t parent = FAT16_ROOT_CLUSTER;
    char name[13] = {0};

    /* split parent + name */
    char tmp[256];
    strncpy(tmp, res.rel_path, sizeof(tmp));
    tmp[sizeof(tmp)-1] = 0;

    char* slash = strrchr(tmp, '/');
    if (slash) {
        *slash = 0;
        strncpy(name, slash + 1, sizeof(name));

        if (*tmp) {
            fat16_dir_entry_t p;
            if (fat16_find_path(fs, tmp, &p) != 0)
                return -1;
            parent = p.first_cluster;
        }
    } else {
        strncpy(name, tmp, sizeof(name));
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

int vfs_cd(const char* path)
{
    if (!path || !*path) {
        eprintf("cd: path is null or undefined");
        return -1;
    }

    char norm[256];
    vfs_normalize_path(path, norm);

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -1;

    if (res.mnt->type != FS_FAT16) {
        eprintf("cd: unknown filesystem");
        return -2;
    }

    fat16_fs_t* fs = (fat16_fs_t*)res.mnt->fs;

    uint16_t new_cluster = FAT16_ROOT_CLUSTER;

    if (*res.rel_path) {
        fat16_dir_entry_t e;
        if (fat16_find_path(fs, res.rel_path, &e) != FAT_OK)
            return -3;

        if (!(e.attr & 0x10))
            return -4;

        new_cluster = e.first_cluster;
    }

    /* COMMIT CWD */
    vfs_cwd_cluster = new_cluster;
    strncpy(vfs_cwd, norm, sizeof(vfs_cwd));
    vfs_cwd[sizeof(vfs_cwd) - 1] = 0;
    return 0;
}


int vfs_create_path(const char* path, uint8_t attr) {
    if (!path || !*path) {
        eprintf("create_path: path is null or undefined");
        return -1;
    }

    char norm[256];
    vfs_normalize_path(path, norm);

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0) return -1;
    if (res.mnt->type == FS_FAT16) {
        fat16_fs_t* fs = (fat16_fs_t*)res.mnt->fs;
        uint16_t parent_cluster = FAT16_ROOT_CLUSTER; // root
        return fat16_create_path(fs, res.rel_path, parent_cluster, attr);
    }

    return -2;
}

int vfs_unlink(const char* path)
{
    if (!path || !*path) {
        eprintf("unlink:: path is null or undefined");
        return -1;
    }
    /* Normalize */
    char norm[256];
    vfs_normalize_path(path, norm);

    /* Resolve mount */
    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -2;

    if (res.mnt->type != FS_FAT16)
        return -3;

    fat16_fs_t* fs = (fat16_fs_t*)res.mnt->fs;

    /* Split parent + name (RELATIVE PATH, NO LEADING '/') */
    char parent_path[256];
    char name[13];

    const char* last = strrchr(res.rel_path, '/');

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

int vfs_mv(const char* src, const char* dst)
{
    char src_norm[256], dst_norm[256];
    vfs_normalize_path(src, src_norm);
    vfs_normalize_path(dst, dst_norm);

    vfs_mount_res_t src_res, dst_res;
    if (vfs_resolve_mount(src_norm, &src_res) != 0) return -1;
    if (vfs_resolve_mount(dst_norm, &dst_res) != 0) return -1;

    if (src_res.mnt != dst_res.mnt) {
        printf("mv: cross-device move not supported");
        return -1;
    }

    if (src_res.mnt->type != FS_FAT16)
        return -1;

    fat16_fs_t* fs = (fat16_fs_t*)src_res.mnt->fs;

    /* ---------- SPLIT SRC ---------- */
    uint16_t src_parent = FAT16_ROOT_CLUSTER;
    char src_name[13];

    char src_tmp[256];
    strncpy(src_tmp, src_res.rel_path, sizeof(src_tmp));
    src_tmp[sizeof(src_tmp)-1] = 0;

    char* s = strrchr(src_tmp, '/');
    if (s) {
        *s = 0;
        strncpy(src_name, s + 1, sizeof(src_name));
        if (*src_tmp) {
            fat16_dir_entry_t e;
            if (fat16_find_path(fs, src_tmp, &e) != 0)
                return -1;
            src_parent = e.first_cluster;
        }
    } else {
        strncpy(src_name, src_tmp, sizeof(src_name));
    }

    /* ---------- SPLIT DST ---------- */
    uint16_t dst_parent = FAT16_ROOT_CLUSTER;
    char dst_name[13];

    char dst_tmp[256];
    strncpy(dst_tmp, dst_res.rel_path, sizeof(dst_tmp));
    dst_tmp[sizeof(dst_tmp)-1] = 0;

    char* d = strrchr(dst_tmp, '/');
    if (d) {
        *d = 0;
        strncpy(dst_name, d + 1, sizeof(dst_name));
        if (*dst_tmp) {
            fat16_dir_entry_t e;
            if (fat16_find_path(fs, dst_tmp, &e) != 0)
                return -1;
            dst_parent = e.first_cluster;
        }
    } else {
        strncpy(dst_name, dst_tmp, sizeof(dst_name));
    }

    /* ---------- REAL MOVE ---------- */
    return fat16_mv(fs, src_parent, src_name, dst_parent, dst_name);
}

const char* vfs_getcwd(void) {
    return vfs_cwd;
}

int vfs_is_direct_child_mount(const char* parent, mount_entry_t* m) {
    if (!strcmp(parent, "/")) {
        if (m->mount_point[0] != '/')
            return 0;
        /* Only one slash allowed */
        const char* rest = m->mount_point + 1;
        return strchr(rest, '/') == NULL;
    }

    size_t plen = strlen(parent);
    if (strncmp(m->mount_point, parent, plen) != 0)
        return 0;

    if (m->mount_point[plen] != '/')
        return 0;

    return strchr(m->mount_point + plen + 1, '/') == NULL;
}

const char* vfs_basename(const char* path) {
    const char* last = path;
    while (*path) {
        if (*path == '/')
            last = path + 1;
        path++;
    }
    return last;
}