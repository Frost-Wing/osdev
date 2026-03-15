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
#include <userland.h>
#include <filesystems/vfs.h>
#include <executables/elf.h>

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
#define LINUX_EINVAL     22
#define LINUX_ENOTTY     25
#define LINUX_ENOSYS     38
#define LINUX_ENFILE     23
#define LINUX_ENOENT     2

#define FW_SYS_GETC      0x1000
#define FW_SYS_GETC_NB   0x1001
#define FW_SYS_PUTC      0x1002
#define FW_SYS_LOGIN     0x1055

#define PROC_MAX_FDS     32
#define FD_STDIN         0
#define FD_STDOUT        1
#define FD_STDERR        2

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
    bool used;
    bool is_stdio;
    vfs_file_t file;
} fd_entry_t;

static fd_entry_t fd_table[PROC_MAX_FDS];
static bool fd_table_initialized = false;

static void fd_table_init(void) {
    if (fd_table_initialized)
        return;

    memset(fd_table, 0, sizeof(fd_table));
    fd_table[FD_STDIN].used = true;
    fd_table[FD_STDIN].is_stdio = true;
    fd_table[FD_STDOUT].used = true;
    fd_table[FD_STDOUT].is_stdio = true;
    fd_table[FD_STDERR].used = true;
    fd_table[FD_STDERR].is_stdio = true;

    fd_table_initialized = true;
}

static int fd_alloc(void) {
    for (int fd = 3; fd < PROC_MAX_FDS; ++fd) {
        if (!fd_table[fd].used) {
            fd_table[fd].used = true;
            fd_table[fd].is_stdio = false;
            memset(&fd_table[fd].file, 0, sizeof(vfs_file_t));
            return fd;
        }
    }

    return -1;
}

static bool fd_valid(int fd) {
    return fd >= 0 && fd < PROC_MAX_FDS && fd_table[fd].used;
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

static uint32_t fd_file_size(vfs_file_t* file) {
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

static uint32_t* fd_pos_ptr(vfs_file_t* file) {
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

static int64 sys_open_common(int dirfd, const char* path, int flags, int mode) {
    (void)mode;
    fd_table_init();

    if (path == NULL)
        return -LINUX_EINVAL;

    if (dirfd != LINUX_AT_FDCWD && dirfd != 0)
        return -LINUX_EINVAL;

    int fd = fd_alloc();
    if (fd < 0)
        return -LINUX_ENFILE;

    int ret = vfs_open(path, linux_flags_to_vfs(flags), &fd_table[fd].file);
    if (ret != 0) {
        fd_table[fd].used = false;
        return -LINUX_ENOENT;
    }

    return fd;
}

static int64 sys_close(uint64_t fd) {
    fd_table_init();

    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    if ((int)fd >= 3)
        vfs_close(&fd_table[fd].file);

    if ((int)fd >= 3)
        memset(&fd_table[fd], 0, sizeof(fd_entry_t));

    return 0;
}

static int64 sys_read(uint64_t fd, char* buf, uint64_t count) {
    fd_table_init();

    if (buf == NULL || count == 0)
        return 0;

    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    if ((int)fd == FD_STDIN) {
        for (uint64_t i = 0; i < count; ++i) {
            char c = getc();
            buf[i] = c;
            if (c == '\n' || c == '\r')
                return (int64)(i + 1);
        }
        return (int64)count;
    }

    if ((int)fd == FD_STDOUT || (int)fd == FD_STDERR)
        return -LINUX_EBADF;

    int rd = vfs_read(&fd_table[fd].file, (uint8_t*)buf, (uint32_t)count);
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

    if ((int)fd == FD_STDOUT || (int)fd == FD_STDERR) {
        for (uint64_t i = 0; i < count; ++i)
            putc(buf[i]);
        return (int64)count;
    }

    if ((int)fd == FD_STDIN)
        return -LINUX_EBADF;

    int wr = vfs_write(&fd_table[fd].file, (const uint8_t*)buf, (uint32_t)count);
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

static int64 sys_lseek(uint64_t fd, int64_t offset, uint64_t whence) {
    fd_table_init();

    if (!fd_valid((int)fd) || (int)fd < 3)
        return -LINUX_EBADF;

    uint32_t* pos = fd_pos_ptr(&fd_table[fd].file);
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
            base = fd_file_size(&fd_table[fd].file);
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

    if (newfd >= PROC_MAX_FDS)
        return -LINUX_EBADF;

    if (oldfd == newfd)
        return (int64)newfd;

    if (fd_valid((int)newfd))
        sys_close(newfd);

    fd_table[newfd] = fd_table[oldfd];
    fd_table[newfd].used = true;

    return (int64)newfd;
}

static int64 sys_dup(uint64_t oldfd) {
    fd_table_init();

    if (!fd_valid((int)oldfd))
        return -LINUX_EBADF;

    int newfd = fd_alloc();
    if (newfd < 0)
        return -LINUX_ENFILE;

    fd_table[newfd] = fd_table[oldfd];
    fd_table[newfd].used = true;

    return newfd;
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

static int64 sys_uname(linux_utsname_t* uts) {
    if (uts == NULL)
        return -LINUX_EINVAL;

    memset(uts, 0, sizeof(*uts));
    memcpy(uts->sysname, "FwOS", 5);
    memcpy(uts->nodename, "fwos", 5);
    memcpy(uts->release, "0.1", 4);
    memcpy(uts->version, "fw-kernel", 10);
    memcpy(uts->machine, "x86_64", 7);
    memcpy(uts->domainname, "localdomain", 12);

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

static int64 sys_brk(uint64_t requested_break) {
    return (int64)userland_brk(requested_break);
}

static int64 sys_munmap(uint64_t addr, uint64_t length) {
    (void)addr;
    (void)length;
    return 0;
}

static int64 sys_execve(const char* target, char* const* argv, char* const* envp) {
    (void)argv;
    (void)envp;

    if (!target)
        return -LINUX_EINVAL;

    void* entry = elf_load_from_vfs(target);
    if (entry) {
        enter_userland_at((uint64_t)entry);
        return 0;
    }

    return execute_chain(target);
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
        case LINUX_SYS_OPENAT:
            frame->rax = sys_open_common((int)frame->rdi, (const char*)frame->rsi, frame->rdx, frame->r10);
            break;
        case LINUX_SYS_CLOSE:
            frame->rax = sys_close(frame->rdi);
            break;
        case LINUX_SYS_LSEEK:
            frame->rax = sys_lseek(frame->rdi, (int64_t)frame->rsi, frame->rdx);
            break;
        case LINUX_SYS_MMAP:
            frame->rax = sys_mmap(frame->rdi, frame->rsi, frame->rdx, frame->rcx, frame->r8, frame->r9);
            break;
        case LINUX_SYS_MUNMAP:
            frame->rax = sys_munmap(frame->rdi, frame->rsi);
            break;
        case LINUX_SYS_BRK:
            frame->rax = sys_brk(frame->rdi);
            break;
        case LINUX_SYS_IOCTL:
            frame->rax = -LINUX_ENOTTY;
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
        case LINUX_SYS_CHDIR:
            frame->rax = sys_chdir((const char*)frame->rdi);
            break;
        case LINUX_SYS_UNAME:
            frame->rax = sys_uname((linux_utsname_t*)frame->rdi);
            break;
        case LINUX_SYS_GETUID:
        case LINUX_SYS_GETGID:
            frame->rax = 0;
            break;
        case LINUX_SYS_ARCH_PRCTL:
            frame->rax = -LINUX_ENOSYS;
            break;
        case FW_SYS_GETC:
            frame->rax = getc();
            break;
        case FW_SYS_GETC_NB:
            frame->rax = kgetc_nonblock();
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
            frame->rax = -LINUX_ENOSYS;
            break;
    }

    outb(0x20, 0x20); // End PIC Master
    outb(0xA0, 0x20); // End PIC Slave
}
