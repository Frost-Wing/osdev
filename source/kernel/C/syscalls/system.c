#include "internal.h"

uint64 sys_access_common(int dirfd, const char *path, int mode) {
    (void)mode;
    if (!path)
        return -LINUX_EINVAL;
    vfs_stat_info_t info;
    return fill_vfs_stat_for_path_at(dirfd, path, &info) ? 0 : -LINUX_ENOENT;
}

uint64 sys_lseek(uint64_t fd, int64_t offset, uint64_t whence) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    uint32_t *pos = fd_pos_ptr((int)fd);
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

uint64 sys_dup2(uint64_t oldfd, uint64_t newfd) {
    if (!fd_valid((int)oldfd))
        return -LINUX_EBADF;

    if (newfd >= STREAM_MAX_FDS)
        return -LINUX_EBADF;

    int rc = fd_dup2((int)oldfd, (int)newfd);
    return rc < 0 ? -LINUX_EBADF : (uint64)rc;
}

uint64 sys_dup(uint64_t oldfd) {
    if (!fd_valid((int)oldfd))
        return -LINUX_EBADF;

    int newfd = fd_dup((int)oldfd);
    return newfd < 0 ? -LINUX_ENFILE : newfd;
}

uint64 sys_getcwd(char *buf, uint64_t size) {
    const char *cwd = vfs_getcwd();
    if (!buf || size == 0)
        return -LINUX_EINVAL;

    uint64_t len = strlen(cwd);
    if (len + 1 > size)
        return -LINUX_EINVAL;

    memcpy(buf, cwd, len + 1);
    return (uint64)len;
}

uint64 sys_chdir(const char *path) {
    if (!path)
        return -LINUX_EINVAL;

    int rc = vfs_cd(path);
    if (rc != 0)
        return -LINUX_ENOENT;

    return 0;
}

uint64 sys_readlinkat(int dirfd,
    const char *path,
    char *buf,
    uint64_t bufsiz) {
    if (!path)
        return -LINUX_EINVAL;

    char resolved_path[256];
    if (!resolve_path_at(dirfd, path, resolved_path, sizeof(resolved_path)))
        return -LINUX_EINVAL;

    if (strcmp(resolved_path, "/proc/self/exe") == 0 ||
        strcmp(resolved_path, "self/exe") == 0) {
        uint32_t pid = multitasking_current_pid();

        if (pid == 0)
            return -LINUX_ENOENT;

        // IMPORTANT: use your existing function
        task_info_t info;

        if (!multitasking_get_task(pid, &info))
            return -LINUX_ENOENT;

        if (!info.name)
            return -LINUX_ENOENT;

        return copy_readlink_result(info.name, buf, bufsiz);
    }

    return -LINUX_ENOENT;
}

uint64 sys_clock_gettime(uint64_t clockid, linux_timespec_t *tp) {
    if (!tp)
        return -LINUX_EINVAL;
    if (clockid != LINUX_CLOCK_REALTIME && clockid != LINUX_CLOCK_MONOTONIC)
        return -LINUX_EINVAL;

    uint8 sec = 0, min = 0, hour = 0, day = 0, month = 0;
    uint16 year = 0;
    update_system_time(&sec, &min, &hour, &day, &month, &year);

    tp->tv_sec = (hour * 3600) + (min * 60) + sec;
    tp->tv_nsec = 0;
    return 0;
}

uint64 sys_nanosleep(const linux_timespec_t *req, linux_timespec_t *rem) {
    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    if (!req)
        return -LINUX_EFAULT;
    if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L)
        return -LINUX_EINVAL;
    if (req->tv_sec > 0)
        sleep((int)req->tv_sec);
    else
        multitasking_yield();
    return 0;
}

int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg) {
    (void)arg;

    if (magic1 != 0xfee1dead)
        return -LINUX_EINVAL;

    switch (magic2) {
        case 672274793:
        case 85072278:
        case 369367448:
        case 537993216:
            break;
        default:
            return -LINUX_EINVAL;
    }

    switch (cmd) {
        case 0x01234567: /* RESTART */
            reboot();
            break;

        case 0x4321FEDC: /* POWER_OFF */
            shutdown();
            break;

        case 0xCDEF0123: /* HALT */
            hcf();
            break;

        default:
            return -LINUX_EINVAL;
    }

    /* Should never return */
    return 0;
}

int sys_kill(int pid, int sig) {
    if (sig < 0 || sig > 64)
        return -LINUX_EINVAL;
    if (pid <= 0)
        return -LINUX_ENOSYS;
    if (sig == 0)
        return multitasking_get_task((uint32_t)pid, &(task_info_t){0}) ? 0 : -LINUX_ESRCH;

    if (pid == 1) {
        switch (sig) {
            case SIGTERM:
            case SIGINT:
                reboot();
                break;

            case SIGKILL:
                shutdown();
                break;
        }

        return 0;
    }

    if (!multitasking_kill_task((uint32_t)pid, sig))
        return -LINUX_ESRCH;
    return 0;
}

// Backs dmesg, which opens /dev/kmsg or falls back to the syslog(2) syscall
// (SYS_syslog == 103 on x86_64) to read the kernel ring buffer.
uint64 sys_syslog(int type, char *buf, uint64_t len) {
    syslog_printf("[syscall] klog: type -> %d", type);
    switch (type) {
        case LINUX_SYSLOG_ACTION_CLOSE:
        case LINUX_SYSLOG_ACTION_OPEN:
            return 0;

        case LINUX_SYSLOG_ACTION_READ:
        case LINUX_SYSLOG_ACTION_READ_ALL:
        case LINUX_SYSLOG_ACTION_READ_CLEAR: {
            syslog_printf("[syscall] klog read: buf=%x len=%x", (unsigned)(uintptr_t)buf, len);
            if (!buf || len <= 0)
                return -LINUX_EINVAL;

            size_t n = klog_read(buf, len);
            syslog_printf("[syscall] klog read: n -> %u", (unsigned)n);

            if (type == LINUX_SYSLOG_ACTION_READ_CLEAR)
                klog_clear();

            return (uint64)n;
        }

        case LINUX_SYSLOG_ACTION_CLEAR:
            klog_clear();
            return 0;

        case LINUX_SYSLOG_ACTION_CONSOLE_OFF:
        case LINUX_SYSLOG_ACTION_CONSOLE_ON:
        case LINUX_SYSLOG_ACTION_CONSOLE_LEVEL:
            return 0;

        case LINUX_SYSLOG_ACTION_SIZE_UNREAD:
        case LINUX_SYSLOG_ACTION_SIZE_BUFFER:
            size_t sz = klog_size();
            syslog_printf("[syscall] klog: size -> %u", (unsigned)sz);
            return (uint64)sz;

        default:
            return -LINUX_EINVAL;
    }
}

/**
 * @brief Linux-compatible mmap wrapper backed by the kernel's anonymous mapper.
 *
 * Anonymous mappings and simple private file-backed mappings are supported.
 * File-backed mappings are eagerly copied so user-space ELF interpreters can
 * map shared-library segments before applying their own relocations.
 */
uint64 sys_mmap(uint64_t addr, uint64_t length, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t off) {
    if (length == 0)
        return -LINUX_EINVAL;

    if ((off & 0xFFFULL) != 0)
        return -LINUX_EINVAL;

    if ((prot & ~(LINUX_PROT_READ | LINUX_PROT_WRITE | LINUX_PROT_EXEC)) != 0)
        return -LINUX_EINVAL;

    if ((flags & (LINUX_MAP_PRIVATE | LINUX_MAP_SHARED)) == 0)
        return -LINUX_EINVAL;

    uint64_t mapped = 0;
    bool anonymous = (flags & LINUX_MAP_ANONYMOUS) != 0;

    if (anonymous) {
        if ((int64_t)fd != -1)
            return -LINUX_EBADF;
        mapped = (flags & LINUX_MAP_FIXED) ? userland_mmap_fixed(addr, length) : userland_mmap_anon(length);
    } else {
        if (!fd_valid((int)fd))
            return -LINUX_EBADF;
        mapped = (flags & LINUX_MAP_FIXED) ? userland_mmap_fixed(addr, length) : userland_mmap_anon(length);
        if (mapped != 0) {
            vfs_file_t *file = fd_get_file((int)fd);
            uint32_t *pos = fd_pos_ptr((int)fd);
            if (!file || !pos)
                return -LINUX_EBADF;
            uint32_t old_pos = *pos;
            *pos = (uint32_t)off;
            int rd = vfs_read(file, (uint8_t *)mapped, (uint32_t)length);
            *pos = old_pos;
            if (rd < 0)
                return -LINUX_EIO;
            if ((uint64_t)rd < length)
                memset((uint8_t *)mapped + rd, 0, length - (uint64_t)rd);
        }
    }

    if (mapped == 0)
        return -LINUX_ENOMEM;

    return (uint64)mapped;
}

/**
 * @brief Linux-compatible mprotect validation wrapper.
 *
 * The current VM layer maps user pages with a single userspace permission
 * template, so mprotect is accepted as a successful no-op after validating
 * address, size, and protection mask.
 */
uint64 sys_mprotect(uint64_t addr, uint64_t length, uint64_t prot) {
    if (length == 0)
        return 0;

    if ((prot & ~(LINUX_PROT_READ | LINUX_PROT_WRITE | LINUX_PROT_EXEC)) != 0)
        return -LINUX_EINVAL;

    if ((addr & 0xFFFULL) != 0)
        return -LINUX_EINVAL;

    uint64_t end = addr + length;
    if (end < addr)
        return -LINUX_EINVAL;

    bool in_image = (addr >= USER_CODE_VADDR && end <= USER_HEAP_VADDR);
    bool in_heap = (addr >= USER_HEAP_VADDR && end <= (USER_HEAP_VADDR + USER_HEAP_SIZE));
    bool in_mmap = (addr >= USER_MMAP_VADDR && end <= (USER_MMAP_VADDR + USER_MMAP_SIZE));
    if (!in_image && !in_heap && !in_mmap)
        return -LINUX_EINVAL;

    return 0;
}

uint64 sys_brk(uint64_t requested_break) {
    return (uint64)userland_brk(requested_break);
}

uint64 sys_munmap(uint64_t addr, uint64_t length) {
    if (length == 0)
        return -LINUX_EINVAL;

    if ((addr & 0xFFFULL) != 0)
        return -LINUX_EINVAL;

    uint64_t end = addr + length;
    if (end < addr)
        return -LINUX_EINVAL;

    bool in_image = (addr >= USER_CODE_VADDR && end <= USER_HEAP_VADDR);
    bool in_heap = (addr >= USER_HEAP_VADDR && end <= (USER_HEAP_VADDR + USER_HEAP_SIZE));
    bool in_mmap = (addr >= USER_MMAP_VADDR && end <= (USER_MMAP_VADDR + USER_MMAP_SIZE));
    if (!in_image && !in_heap && !in_mmap)
        return -LINUX_EINVAL;

    return 0;
}

uint64 sys_arch_prctl(uint64_t code, uint64_t addr) {
    switch (code) {
        case LINUX_ARCH_SET_FS:
            current_fs_base = addr;
            wrmsr64(IA32_FS_BASE_MSR, addr);
            return 0;
        case LINUX_ARCH_GET_FS:
            if (!addr)
                return -LINUX_EINVAL;
            *(uint64_t *)addr = rdmsr64(IA32_FS_BASE_MSR);
            return 0;
        default:
            return -LINUX_ENOSYS;
    }
}

uint64 sys_prlimit64(uint64_t pid, uint64_t resource, const linux_rlimit64_t *new_limit, linux_rlimit64_t *old_limit) {
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

uint64 sys_umask(uint64_t mask) {
    uint32_t previous = current_umask;
    current_umask = (uint32_t)(mask & 0777U);
    return (uint64)previous;
}

uint64 sys_tgkill(uint64_t tgid, uint64_t tid, uint64_t sig) {
    if (sig == 0)
        return 0;
    if (sig > 64)
        return -LINUX_EINVAL;
    if (tgid != 1 || tid != 1)
        return -LINUX_ESRCH;
    return -LINUX_ENOSYS;
}

uint64 sys_set_tid_address(uint64_t *tidptr) {
    clear_child_tid = tidptr;
    return 1;
}

uint64 sys_set_robust_list(const void *head, uint64_t len) {
    (void)head;
    if (len != 24)
        return -LINUX_EINVAL;
    return 0;
}

uint64 sys_getrandom(void *buf, uint64_t buflen, uint64_t flags) {
    (void)flags;
    if (!buf)
        return -LINUX_EINVAL;
    uint8_t *out = (uint8_t *)buf;
    uint64_t state = rdtsc64();
    for (uint64_t i = 0; i < buflen; ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        out[i] = (uint8_t)(state >> (i & 7));
    }
    return (uint64)buflen;
}
