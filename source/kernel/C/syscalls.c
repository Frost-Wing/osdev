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

extern struct limine_framebuffer *framebuffer;
extern int64* font_address;

extern int execute_chain(const char* line);
extern bool running; // from sh.c

#define LINUX_EINVAL 22
#define LINUX_ENOTTY 25
#define LINUX_ENOSYS 38

#define FW_SYS_GETC        0x100
#define FW_SYS_GETC_NB     0x101
#define FW_SYS_PUTC        0x102
#define FW_SYS_LOGIN       0x155

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

static int64 sys_read(uint64_t fd, char* buf, uint64_t count) {
    if (buf == NULL || count == 0) {
        return 0;
    }

    if (fd != 0) {
        return -1;
    }

    for (uint64_t i = 0; i < count; ++i) {
        char c = getc();
        buf[i] = c;
        if (c == '\n' || c == '\r') {
            return (int64)(i + 1);
        }
    }

    return (int64)count;
}

static int64 sys_write(uint64_t fd, const char* buf, uint64_t count) {
    if (buf == NULL || count == 0) {
        return 0;
    }

    if (fd > 2) {
        return -1;
    }

    for (uint64_t i = 0; i < count; ++i) {
        putc(buf[i]);
    }

    return (int64)count;
}

static int64 sys_writev(uint64_t fd, const linux_iovec_t* iov, uint64_t iovcnt) {
    if (iov == NULL) {
        return -LINUX_EINVAL;
    }

    int64 total = 0;
    for (uint64_t i = 0; i < iovcnt; ++i) {
        int64 written = sys_write(fd, (const char*)iov[i].iov_base, iov[i].iov_len);
        if (written < 0) {
            return written;
        }
        total += written;
    }

    return total;
}

static int64 sys_uname(linux_utsname_t* uts) {
    if (uts == NULL) {
        return -LINUX_EINVAL;
    }

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
    if (mapped == 0) {
        return -LINUX_EINVAL;
    }

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
        case LINUX_SYS_GETPID:
            frame->rax = 1;
            break;
        case LINUX_SYS_EXECVE:
            frame->rax = execute_chain((const char*)frame->rdi);
            break;
        case LINUX_SYS_EXIT:
        case LINUX_SYS_EXIT_GROUP: {
            int code = frame->rdi;
            printf(blue_color "\n[process exited with code %d]" reset_color, code);
            running = false;
            frame->rax = 0;
            break;
        }
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
