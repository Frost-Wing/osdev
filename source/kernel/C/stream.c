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

#include <basics.h>
#include <filesystems/vfs.h>
#include <flanterm/flanterm.h>
#include <graphics.h> // flanterm
#include <memory.h>
#include <multitasking.h>
#include <stream.h>

extern struct flanterm_context *ft_ctx;

typedef struct {
    int index;
    vfs_file_t *file; // NULL → terminal
} stream_impl_t;

typedef struct {
    bool used;
    size_t read_pos;
    size_t write_pos;
    size_t count;
    size_t readers;
    size_t writers;
    uint8_t data[4096];
} fd_pipe_t;

typedef struct {
    bool used;
    int ref_count;
    bool owns_file;
    int flags;
    bool tty;
    vfs_file_t *file;
    vfs_file_t storage;
    char path[256];
    fd_pipe_t *pipe;
    bool pipe_reader;
    bool eventfd;
    bool eventfd_semaphore;
    bool eventfd_nonblocking;
    uint64_t eventfd_counter;
} fd_object_t;

typedef struct {
    bool used;
    fd_object_t *object;
} fd_entry_t;

static stream_impl_t streams[3];
static fd_entry_t fd_table[STREAM_MAX_FDS];
static fd_object_t fd_objects[STREAM_MAX_FDS];
static fd_pipe_t fd_pipes[16];
static bool fd_initialized = false;

static fd_object_t *fd_object_alloc(vfs_file_t *file, bool owns_file, int flags) {
    for (int i = 0; i < STREAM_MAX_FDS; ++i) {
        if (!fd_objects[i].used) {
            fd_objects[i].used = true;
            fd_objects[i].ref_count = 1;
            fd_objects[i].owns_file = owns_file;
            fd_objects[i].flags = flags;
            fd_objects[i].tty = false;
            fd_objects[i].file = file;
            memset(fd_objects[i].path, 0, sizeof(fd_objects[i].path));

            if (!owns_file)
                memset(&fd_objects[i].storage, 0, sizeof(vfs_file_t));

            return &fd_objects[i];
        }
    }

    return NULL;
}

static void fd_object_retain(fd_object_t *object) {
    if (object)
        object->ref_count++;
}

static void fd_object_release(fd_object_t *object) {
    if (!object || !object->used)
        return;

    object->ref_count--;
    if (object->ref_count > 0)
        return;

    if (object->pipe) {
        if (object->pipe_reader)
            object->pipe->readers--;
        else
            object->pipe->writers--;
        if (object->pipe->readers == 0 && object->pipe->writers == 0)
            memset(object->pipe, 0, sizeof(*object->pipe));
    }

    if (object->owns_file && object->file && object->file->mnt)
        vfs_close(object->file);

    memset(object, 0, sizeof(*object));
}

static int fd_alloc_slot(void) {
    for (int fd = 3; fd < STREAM_MAX_FDS; ++fd) {
        if (!fd_table[fd].used)
            return fd;
    }

    return -1;
}

static void fd_assign_slot(int fd, fd_object_t *object) {
    if (fd < 0 || fd >= STREAM_MAX_FDS)
        return;

    if (fd_table[fd].used)
        fd_object_release(fd_table[fd].object);

    fd_table[fd].used = true;
    fd_table[fd].object = object;
    fd_object_retain(object);
}

void stream_init(void) {
    fd_table_init();
}

int stream_set_file(stream_t s, vfs_file_t *file) {
    streams[s].file = file;
    streams[s].index = s;

    int flags = (s == STDIN) ? VFS_RDONLY : VFS_WRONLY;
    fd_object_t *object = fd_object_alloc(file, false, flags);
    if (!object)
        return -1;
    object->tty = file == NULL;

    fd_assign_slot((int)s, object);
    fd_object_release(object);

    return (int)s;
}

vfs_file_t *stream_get_file(stream_t s) {
    return streams[s].file;
}

void stream_write(stream_t s, const char *buf, size_t len) {
    if (!buf || len == 0)
        return;

    stream_impl_t *st = &streams[s];

    if (st->file) {
        vfs_write(st->file, (const uint8_t *)buf, len);
    } else {
        flanterm_write(ft_ctx, buf, len);
    }
}

void stream_putc(stream_t s, char c) {
    char str[2];
    str[0] = c;
    str[1] = '\0';
    stream_write(s, str, 1);
}

void fd_table_init(void) {
    if (fd_initialized)
        return;

    memset(streams, 0, sizeof(streams));
    memset(fd_table, 0, sizeof(fd_table));
    memset(fd_objects, 0, sizeof(fd_objects));
    memset(fd_pipes, 0, sizeof(fd_pipes));

    for (int i = STDIN; i <= STDERR; ++i) {
        streams[i].index = i;
        streams[i].file = NULL;

        int flags = (i == STDIN) ? VFS_RDONLY : VFS_WRONLY;
        fd_object_t *object = fd_object_alloc(NULL, false, flags);
        if (!object)
            return;

        object->tty = true;
        fd_table[i].used = true;
        fd_table[i].object = object;
    }

    fd_initialized = true;
}

bool fd_valid(int fd) {
    return fd >= 0 && fd < STREAM_MAX_FDS && fd_table[fd].used;
}

bool fd_is_tty(int fd) {
    return fd_valid(fd) && fd_table[fd].object &&
        fd_table[fd].object->tty;
}

vfs_file_t *fd_get_file(int fd) {
    if (!fd_valid(fd))
        return NULL;

    if (!fd_table[fd].object)
        return NULL;

    return fd_table[fd].object->file;
}

int fd_open(const char *path, int flags) {
    int fd = fd_alloc_slot();
    if (fd < 0)
        return -1;

    fd_object_t *object = fd_object_alloc(NULL, true, flags);
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

int fd_create_virtual(const char *path, int flags) {
    int fd = fd_alloc_slot();
    if (fd < 0)
        return -1;

    fd_object_t *object = fd_object_alloc(NULL, false, flags);
    if (!object)
        return -1;

    if (path)
        vfs_normalize_path(path, object->path, sizeof(object->path));

    fd_table[fd].used = true;
    fd_table[fd].object = object;
    return fd;
}

int fd_close(int fd) {
    if (!fd_valid(fd))
        return -1;

    fd_object_release(fd_table[fd].object);
    fd_table[fd].used = false;
    fd_table[fd].object = NULL;

    return 0;
}

int fd_dup2(int oldfd, int newfd) {
    if (!fd_valid(oldfd) || newfd < 0 || newfd >= STREAM_MAX_FDS)
        return -1;

    if (oldfd == newfd)
        return newfd;

    if (fd_table[newfd].used)
        fd_close(newfd);

    fd_assign_slot(newfd, fd_table[oldfd].object);
    return newfd;
}

int fd_dup(int oldfd) {
    if (!fd_valid(oldfd))
        return -1;

    int newfd = fd_alloc_slot();
    if (newfd < 0)
        return -1;

    fd_assign_slot(newfd, fd_table[oldfd].object);
    return newfd;
}

int fd_flags(int fd) {
    if (!fd_valid(fd) || !fd_table[fd].object)
        return 0;

    return fd_table[fd].object->flags;
}

const char *fd_get_path(int fd) {
    if (!fd_valid(fd) || !fd_table[fd].object)
        return NULL;

    if (fd_table[fd].object->path[0] == '\0')
        return NULL;

    return fd_table[fd].object->path;
}

uint32_t fd_file_size(int fd) {
    vfs_file_t *file = fd_get_file(fd);
    if (!file || !file->mnt)
        return 0;

    switch (file->mnt->type) {
        case FS_FAT16:
            return file->f.fat16.entry.filesize;
        case FS_FAT32:
            return file->f.fat32.entry.file_size;
        case FS_ISO9660:
            return file->f.iso9660.entry.size;
        case FS_EXT2:
            return file->f.ext2.inode.i_size;
        case FS_PROC:
        case FS_DEV:
        default:
            return 0;
    }
}

uint32_t *fd_pos_ptr(int fd) {
    vfs_file_t *file = fd_get_file(fd);
    if (!file || !file->mnt)
        return NULL;

    switch (file->mnt->type) {
        case FS_FAT16:
            return &file->f.fat16.pos;
        case FS_FAT32:
            return &file->f.fat32.pos;
        case FS_ISO9660:
            return &file->f.iso9660.pos;
        case FS_EXT2:
            return &file->f.ext2.pos;
        case FS_PROC:
        case FS_SYS:
        case FS_DEV:
            return &file->pos;
        default:
            return NULL;
    }
}

int fd_pipe_create(int fds[2]) {
    if (!fds)
        return -1;

    int read_fd = fd_alloc_slot();
    if (read_fd < 0)
        return -1;
    int write_fd = -1;
    for (int fd = read_fd + 1; fd < STREAM_MAX_FDS; ++fd) {
        if (!fd_table[fd].used) {
            write_fd = fd;
            break;
        }
    }
    if (write_fd < 0)
        return -1;

    fd_pipe_t *pipe = NULL;
    for (size_t i = 0; i < sizeof(fd_pipes) / sizeof(fd_pipes[0]); ++i) {
        if (!fd_pipes[i].used) {
            pipe = &fd_pipes[i];
            break;
        }
    }
    if (!pipe)
        return -1;

    fd_object_t *reader = fd_object_alloc(NULL, false, VFS_RDONLY);
    fd_object_t *writer = fd_object_alloc(NULL, false, VFS_WRONLY);
    if (!reader || !writer) {
        fd_object_release(reader);
        fd_object_release(writer);
        return -1;
    }

    memset(pipe, 0, sizeof(*pipe));
    pipe->used = true;
    pipe->readers = 1;
    pipe->writers = 1;

    reader->pipe = pipe;
    reader->pipe_reader = true;
    writer->pipe = pipe;
    writer->pipe_reader = false;

    fd_table[read_fd].used = true;
    fd_table[read_fd].object = reader;
    fd_table[write_fd].used = true;
    fd_table[write_fd].object = writer;
    fds[0] = read_fd;
    fds[1] = write_fd;
    return 0;
}

bool fd_is_pipe(int fd) {
    return fd_valid(fd) && fd_table[fd].object &&
        fd_table[fd].object->pipe != NULL;
}

int fd_pipe_read(int fd, void *buf, size_t count) {
    if (!fd_is_pipe(fd) || !fd_table[fd].object->pipe_reader || !buf)
        return -1;

    fd_pipe_t *pipe = fd_table[fd].object->pipe;
    while (pipe->count == 0 && pipe->writers > 0)
        multitasking_yield();
    if (pipe->count == 0)
        return 0;

    size_t bytes = count < pipe->count ? count : pipe->count;
    uint8_t *out = (uint8_t *)buf;
    for (size_t i = 0; i < bytes; ++i) {
        out[i] = pipe->data[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % sizeof(pipe->data);
    }
    pipe->count -= bytes;
    return (int)bytes;
}

int fd_pipe_write(int fd, const void *buf, size_t count) {
    if (!fd_is_pipe(fd) || fd_table[fd].object->pipe_reader || !buf)
        return -1;

    fd_pipe_t *pipe = fd_table[fd].object->pipe;
    const uint8_t *in = (const uint8_t *)buf;
    size_t written = 0;
    while (written < count) {
        if (pipe->readers == 0)
            return written ? (int)written : -1;
        while (pipe->count == sizeof(pipe->data) && pipe->readers > 0)
            multitasking_yield();
        if (pipe->readers == 0)
            return written ? (int)written : -1;

        size_t available = sizeof(pipe->data) - pipe->count;
        size_t bytes = count - written;
        if (bytes > available)
            bytes = available;
        for (size_t i = 0; i < bytes; ++i) {
            pipe->data[pipe->write_pos] = in[written + i];
            pipe->write_pos = (pipe->write_pos + 1) % sizeof(pipe->data);
        }
        pipe->count += bytes;
        written += bytes;
    }
    return (int)written;
}

/* Linux poll bits (ABI constants) */
#define FD_POLLIN  0x001
#define FD_POLLOUT 0x004
#define FD_POLLERR 0x008
#define FD_POLLHUP 0x010

/* Real readiness for a pipe end, as Linux poll bits. Returns 0 if fd isn't a pipe. */
int fd_pipe_poll(int fd) {
    if (!fd_is_pipe(fd))
        return 0;

    fd_object_t *obj = fd_table[fd].object;
    fd_pipe_t *pipe = obj->pipe;
    int r = 0;

    if (obj->pipe_reader) {
        if (pipe->count > 0)
            r |= FD_POLLIN;
        if (pipe->writers == 0)
            r |= FD_POLLHUP;                 /* EOF: read() returns 0 */
    } else {
        if (pipe->readers == 0)
            r |= FD_POLLERR;                 /* write() would get EPIPE */
        else if (pipe->count < sizeof(pipe->data))
            r |= FD_POLLOUT;
    }
    return r;
}

int fd_eventfd_create(uint64_t initial_value, bool semaphore, bool nonblocking) {
    int fd = fd_alloc_slot();
    if (fd < 0)
        return -1;

    fd_object_t *object = fd_object_alloc(NULL, false, VFS_RDWR);
    if (!object)
        return -1;

    object->eventfd = true;
    object->eventfd_semaphore = semaphore;
    object->eventfd_nonblocking = nonblocking;
    object->eventfd_counter = initial_value;
    fd_table[fd].used = true;
    fd_table[fd].object = object;
    return fd;
}

bool fd_is_eventfd(int fd) {
    return fd_valid(fd) && fd_table[fd].object &&
           fd_table[fd].object->eventfd;
}

bool fd_eventfd_nonblocking(int fd) {
    return fd_is_eventfd(fd) &&
           fd_table[fd].object->eventfd_nonblocking;
}

int fd_eventfd_read(int fd, uint64_t *value) {
    if (!fd_is_eventfd(fd) || !value)
        return -1;

    fd_object_t *object = fd_table[fd].object;
    if (object->eventfd_counter == 0)
        return 0;

    *value = object->eventfd_semaphore ? 1 : object->eventfd_counter;
    object->eventfd_counter -= *value;
    return 1;
}

int fd_eventfd_write(int fd, uint64_t value) {
    if (!fd_is_eventfd(fd))
        return -1;

    fd_object_t *object = fd_table[fd].object;
    if (value > UINT64_MAX - 1 - object->eventfd_counter)
        return 0;

    object->eventfd_counter += value;
    return 1;
}

int fd_eventfd_poll(int fd, int events) {
    if (!fd_is_eventfd(fd))
        return 0;

    fd_object_t *object = fd_table[fd].object;
    int ready = 0;
    if (object->eventfd_counter > 0)
        ready |= events & FD_POLLIN;
    if (object->eventfd_counter <= UINT64_MAX - 2)
        ready |= events & FD_POLLOUT;
    return ready;
}
