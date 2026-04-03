/**
 * @file stream.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Unix like standard streams.
 * @version 0.1
 * @date 2026-01-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */

#include <stream.h>
#include <graphics.h>   // flanterm
#include <basics.h>
#include <memory.h>
#include <flanterm/flanterm.h>
#include <filesystems/vfs.h>

extern struct flanterm_context* ft_ctx;

typedef struct {
    int index;
    vfs_file_t* file;   // NULL → terminal
} stream_impl_t;

typedef struct {
    bool used;
    int ref_count;
    bool owns_file;
    int flags;
    vfs_file_t* file;
    vfs_file_t storage;
    char path[256];
} fd_object_t;

typedef struct {
    bool used;
    fd_object_t* object;
} fd_entry_t;

static stream_impl_t streams[3];
static fd_entry_t fd_table[STREAM_MAX_FDS];
static fd_object_t fd_objects[STREAM_MAX_FDS];
static bool fd_initialized = false;

static fd_object_t* fd_object_alloc(vfs_file_t* file, bool owns_file, int flags)
{
    for (int i = 0; i < STREAM_MAX_FDS; ++i) {
        if (!fd_objects[i].used) {
            fd_objects[i].used = true;
            fd_objects[i].ref_count = 1;
            fd_objects[i].owns_file = owns_file;
            fd_objects[i].flags = flags;
            fd_objects[i].file = file;
            memset(fd_objects[i].path, 0, sizeof(fd_objects[i].path));

            if (!owns_file)
                memset(&fd_objects[i].storage, 0, sizeof(vfs_file_t));

            return &fd_objects[i];
        }
    }

    return NULL;
}

static void fd_object_retain(fd_object_t* object)
{
    if (object)
        object->ref_count++;
}

static void fd_object_release(fd_object_t* object)
{
    if (!object || !object->used)
        return;

    object->ref_count--;
    if (object->ref_count > 0)
        return;

    if (object->owns_file && object->file && object->file->mnt)
        vfs_close(object->file);

    memset(object, 0, sizeof(*object));
}

static int fd_alloc_slot(void)
{
    for (int fd = 3; fd < STREAM_MAX_FDS; ++fd) {
        if (!fd_table[fd].used)
            return fd;
    }

    return -1;
}

static void fd_assign_slot(int fd, fd_object_t* object)
{
    if (fd < 0 || fd >= STREAM_MAX_FDS)
        return;

    if (fd_table[fd].used)
        fd_object_release(fd_table[fd].object);

    fd_table[fd].used = true;
    fd_table[fd].object = object;
    fd_object_retain(object);
}

void stream_init(void)
{
    fd_table_init();
}

int stream_set_file(stream_t s, vfs_file_t* file)
{
    streams[s].file = file;
    streams[s].index = s;

    int flags = (s == STDIN) ? VFS_RDONLY : VFS_WRONLY;
    fd_object_t* object = fd_object_alloc(file, false, flags);
    if (!object)
        return -1;

    fd_assign_slot((int)s, object);
    fd_object_release(object);

    return (int)s;
}

vfs_file_t* stream_get_file(stream_t s)
{
    return streams[s].file;
}

void stream_write(stream_t s, const char* buf, size_t len)
{
    if (!buf || len == 0) return;

    stream_impl_t* st = &streams[s];

    if (st->file) {
        vfs_write(st->file, (const uint8_t*)buf, len);
    } else {
        flanterm_write(ft_ctx, buf, len);
    }
}

void stream_putc(stream_t s, char c)
{
    char str[2];
    str[0] = c;
    str[1] = '\0';
    stream_write(s, str, 1);
}

void fd_table_init(void)
{
    if (fd_initialized)
        return;

    memset(streams, 0, sizeof(streams));
    memset(fd_table, 0, sizeof(fd_table));
    memset(fd_objects, 0, sizeof(fd_objects));

    for (int i = STDIN; i <= STDERR; ++i) {
        streams[i].index = i;
        streams[i].file = NULL;

        int flags = (i == STDIN) ? VFS_RDONLY : VFS_WRONLY;
        fd_object_t* object = fd_object_alloc(NULL, false, flags);
        if (!object)
            return;

        fd_table[i].used = true;
        fd_table[i].object = object;
    }

    fd_initialized = true;
}

bool fd_valid(int fd)
{
    return fd >= 0 && fd < STREAM_MAX_FDS && fd_table[fd].used;
}

vfs_file_t* fd_get_file(int fd)
{
    if (!fd_valid(fd))
        return NULL;

    if (!fd_table[fd].object)
        return NULL;

    return fd_table[fd].object->file;
}

int fd_open(const char* path, int flags)
{
    int fd = fd_alloc_slot();
    if (fd < 0)
        return -1;

    fd_object_t* object = fd_object_alloc(NULL, true, flags);
    if (!object)
        return -1;

    object->file = &object->storage;
    memset(object->file, 0, sizeof(vfs_file_t));
    if (path)
        vfs_normalize_path(path, object->path, sizeof(object->path));

    if (vfs_open(path, flags, object->file) != 0) {
        fd_object_release(object);
        return -2;
    }

    fd_table[fd].used = true;
    fd_table[fd].object = object;
    return fd;
}

int fd_close(int fd)
{
    if (!fd_valid(fd))
        return -1;

    fd_object_release(fd_table[fd].object);
    fd_table[fd].used = false;
    fd_table[fd].object = NULL;

    return 0;
}

int fd_dup2(int oldfd, int newfd)
{
    if (!fd_valid(oldfd) || newfd < 0 || newfd >= STREAM_MAX_FDS)
        return -1;

    if (oldfd == newfd)
        return newfd;

    if (fd_table[newfd].used)
        fd_close(newfd);

    fd_assign_slot(newfd, fd_table[oldfd].object);
    return newfd;
}

int fd_dup(int oldfd)
{
    if (!fd_valid(oldfd))
        return -1;

    int newfd = fd_alloc_slot();
    if (newfd < 0)
        return -1;

    fd_assign_slot(newfd, fd_table[oldfd].object);
    return newfd;
}

int fd_flags(int fd)
{
    if (!fd_valid(fd) || !fd_table[fd].object)
        return 0;

    return fd_table[fd].object->flags;
}

const char* fd_get_path(int fd)
{
    if (!fd_valid(fd) || !fd_table[fd].object)
        return NULL;

    if (fd_table[fd].object->path[0] == '\0')
        return NULL;

    return fd_table[fd].object->path;
}

uint32_t fd_file_size(int fd)
{
    vfs_file_t* file = fd_get_file(fd);
    if (!file || !file->mnt)
        return 0;

    switch (file->mnt->type) {
        case FS_FAT16:
            return file->f.fat16.entry.filesize;
        case FS_FAT32:
            return file->f.fat32.entry.file_size;
        case FS_ISO9660:
            return file->f.iso9660.entry.size;
        case FS_PROC:
        default:
            return 0;
    }
}

uint32_t* fd_pos_ptr(int fd)
{
    vfs_file_t* file = fd_get_file(fd);
    if (!file || !file->mnt)
        return NULL;

    switch (file->mnt->type) {
        case FS_FAT16:
            return &file->f.fat16.pos;
        case FS_FAT32:
            return &file->f.fat32.pos;
        case FS_ISO9660:
            return &file->f.iso9660.pos;
        case FS_PROC:
            return &file->pos;
        default:
            return NULL;
    }
}
