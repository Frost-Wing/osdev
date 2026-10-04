#include <syscalls/internal.h>
#include <pit.h>

typedef struct {
    uint32_t cpu_id_start;
    uint32_t cpu_id;
    uint64_t rseq_cs;
    uint32_t flags;
    uint32_t node_id;
    uint32_t mm_cid;
    uint32_t reserved;
} linux_rseq_t;

_Static_assert(sizeof(linux_rseq_t) == 32, "Linux rseq ABI size");

#define LINUX_RSEQ_FLAG_UNREGISTER 1
#define LINUX_RSEQ_CPU_ID_UNINITIALIZED UINT32_MAX
#define LINUX_RSEQ_CPU_ID_REGISTRATION_FAILED (UINT32_MAX - 1)

static uint64_t fallback_rseq_area;
static uint32_t fallback_rseq_signature;

static void rseq_current_state(uint64_t **area, uint32_t **signature) {
    task_t *task = multitasking_get_current_task();
    if (task) {
        *area = &task->rseq_area;
        *signature = &task->rseq_signature;
    } else {
        *area = &fallback_rseq_area;
        *signature = &fallback_rseq_signature;
    }
}

uint64 sys_rseq(void *area, uint64_t length, uint64_t flags, uint32_t signature) {
    if (length != sizeof(linux_rseq_t) ||
        (flags != 0 && flags != LINUX_RSEQ_FLAG_UNREGISTER))
        return -LINUX_EINVAL;

    uint64_t *registered_area;
    uint32_t *registered_signature;
    rseq_current_state(&registered_area, &registered_signature);

    if (flags == LINUX_RSEQ_FLAG_UNREGISTER) {
        if (!area || *registered_area != (uint64_t)area ||
            *registered_signature != signature)
            return -LINUX_EINVAL;

        linux_rseq_t *rseq = area;
        rseq->cpu_id_start = LINUX_RSEQ_CPU_ID_UNINITIALIZED;
        rseq->cpu_id = LINUX_RSEQ_CPU_ID_REGISTRATION_FAILED;
        *registered_area = 0;
        *registered_signature = 0;
        return 0;
    }

    if (!area || ((uintptr_t)area & 31U) != 0)
        return -LINUX_EINVAL;
    if (*registered_area)
        return -LINUX_EBUSY;

    linux_rseq_t *rseq = area;
    rseq->cpu_id_start = 0;
    rseq->cpu_id = 0;
    rseq->node_id = 0;
    rseq->mm_cid = 0;
    *registered_area = (uint64_t)area;
    *registered_signature = signature;
    return 0;
}

void sys_rseq_reset_current(void) {
    uint64_t *registered_area;
    uint32_t *registered_signature;
    rseq_current_state(&registered_area, &registered_signature);
    *registered_area = 0;
    *registered_signature = 0;
}

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

    if (oldfd == newfd)
        return (uint64)newfd;
    sys_socket_close((int)newfd);
    int rc = fd_dup2((int)oldfd, (int)newfd);
    if (rc >= 0)
        sys_socket_dup((int)oldfd, rc);
    return rc < 0 ? -LINUX_EBADF : (uint64)rc;
}

uint64 sys_dup(uint64_t oldfd) {
    if (!fd_valid((int)oldfd))
        return -LINUX_EBADF;

    int newfd = fd_dup((int)oldfd);
    if (newfd >= 0)
        sys_socket_dup((int)oldfd, newfd);
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

#define LINUX_S_IFLNK 0120000

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

    if (!buf || bufsiz == 0)
        return -LINUX_EINVAL;

    int rc = vfs_readlink(resolved_path, buf,
        bufsiz > UINT32_MAX ? UINT32_MAX : (uint32_t)bufsiz);
    if (rc >= 0)
        return (uint64)rc;

    /* Path exists but isn't a symlink -> EINVAL, like Linux.
     * realpath() depends on this to walk path components. */
    vfs_stat_info_t stat_info;
    if (fill_vfs_lstat_for_path_at(LINUX_AT_FDCWD, resolved_path, &stat_info) &&
        (stat_info.mode & 0xF000) != 0120000 /* S_IFLNK */)
        return -LINUX_EINVAL;

    return -LINUX_ENOENT;
}

uint64_t sys_clock_gettime(uint64_t clockid, linux_timespec_t *tp) {
    if (!tp)
        return -LINUX_EINVAL;

    switch (clockid) {
        case LINUX_CLOCK_REALTIME:
            tp->tv_sec = rtc_get_unix_time();
            tp->tv_nsec = 0;
            return 0;

        case LINUX_CLOCK_MONOTONIC: {
            uint64_t ticks = pit_ticks;
            tp->tv_sec = ticks / PIT_TICKS_PER_SECOND;
            tp->tv_nsec = (ticks % PIT_TICKS_PER_SECOND) *
                (1000000000ULL / PIT_TICKS_PER_SECOND);
            return 0;
        }

        default:
            return -LINUX_EINVAL;
    }
}

uint64 sys_time(int64_t *tloc) {
    int64_t now = (int64_t)rtc_get_unix_time();

    if (tloc)
        *tloc = now;

    return (uint64)now;
}

static uint64 sys_update_path_times(int dirfd, const char *path,
    uint64_t atime, uint64_t mtime, bool atime_omit, bool mtime_omit) {
    char normalized[256];
    if (!resolve_path_at(dirfd, path, normalized, sizeof(normalized)))
        return -LINUX_EINVAL;

    if (atime_omit || mtime_omit) {
        vfs_stat_info_t info;
        if (!fill_vfs_stat_for_path_at(LINUX_AT_FDCWD, normalized, &info))
            return -LINUX_ENOENT;
        if (atime_omit)
            atime = (uint64_t)info.atim.tv_sec;
        if (mtime_omit)
            mtime = (uint64_t)info.mtim.tv_sec;
    }
    return sys_set_file_times(normalized, atime, mtime);
}

uint64 sys_utimensat(int dirfd, const char *path,
    const linux_timespec_t *times, int flags) {
    if (!path)
        return -LINUX_EFAULT;
    if (flags & ~(LINUX_AT_SYMLINK_NOFOLLOW | LINUX_AT_EMPTY_PATH))
        return -LINUX_EINVAL;

    uint64_t now = rtc_get_unix_time();
    uint64_t atime = now;
    uint64_t mtime = now;
    bool atime_omit = false;
    bool mtime_omit = false;
    if (times) {
        const long UTIME_NOW_VALUE = 1073741823L;
        const long UTIME_OMIT_VALUE = 1073741822L;
        const linux_timespec_t *at = &times[0];
        const linux_timespec_t *mt = &times[1];
        if (at->tv_nsec == UTIME_NOW_VALUE)
            atime = now;
        else if (at->tv_nsec == UTIME_OMIT_VALUE)
            atime_omit = true;
        else if (at->tv_sec < 0 || at->tv_nsec < 0 || at->tv_nsec >= 1000000000L)
            return -LINUX_EINVAL;
        else
            atime = (uint64_t)at->tv_sec;
        if (mt->tv_nsec == UTIME_NOW_VALUE)
            mtime = now;
        else if (mt->tv_nsec == UTIME_OMIT_VALUE)
            mtime_omit = true;
        else if (mt->tv_sec < 0 || mt->tv_nsec < 0 || mt->tv_nsec >= 1000000000L)
            return -LINUX_EINVAL;
        else
            mtime = (uint64_t)mt->tv_sec;
    }
    return sys_update_path_times(dirfd, path, atime, mtime,
        atime_omit, mtime_omit);
}

static uint64 sys_update_timeval_path(int dirfd, const char *path,
    const void *times) {
    uint64_t now = rtc_get_unix_time();
    uint64_t atime = now;
    uint64_t mtime = now;
    if (times) {
        const int64_t *values = (const int64_t *)times;
        if (values[0] < 0 || values[1] < 0 || values[1] >= 1000000 ||
            values[2] < 0 || values[3] < 0 || values[3] >= 1000000)
            return -LINUX_EINVAL;
        atime = (uint64_t)values[0];
        mtime = (uint64_t)values[2];
    }
    return sys_update_path_times(dirfd, path, atime, mtime, false, false);
}

uint64 sys_utimes(const char *path, const void *times) {
    if (!path)
        return -LINUX_EFAULT;
    return sys_update_timeval_path(LINUX_AT_FDCWD, path, times);
}

uint64 sys_futimesat(int dirfd, const char *path, const void *times) {
    if (!path)
        return -LINUX_EFAULT;
    return sys_update_timeval_path(dirfd, path, times);
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
        /* Linux ignores fd for anonymous mappings. */
        mapped = (flags & LINUX_MAP_FIXED) ? userland_mmap_fixed(addr, length) : userland_mmap_anon(length);
    } else {
        if (!fd_valid((int)fd))
            return -LINUX_EBADF;
        mapped = (flags & LINUX_MAP_FIXED) ? userland_mmap_fixed(addr, length) : userland_mmap_anon(length);
        if (mapped != 0) {
            vfs_file_t *file = fd_get_file((int)fd);
            uint32_t *pos = fd_pos_ptr((int)fd);
            if (!file || !pos) {
                userland_mmap_unmap(mapped, length);
                return -LINUX_EBADF;
            }
            uint32_t old_pos = *pos;
            *pos = (uint32_t)off;
            int rd = vfs_read(file, (uint8_t *)mapped, (uint32_t)length);
            *pos = old_pos;
            if (rd < 0) {
                userland_mmap_unmap(mapped, length);
                return -LINUX_EIO;
            }
            if ((uint64_t)rd < length)
                memset((uint8_t *)mapped + rd, 0, length - (uint64_t)rd);
        }
    }

    if (mapped == 0)
        return -LINUX_ENOMEM;

    if (!userland_mprotect(mapped, length, prot)) {
        userland_mmap_unmap(mapped, length);
        return -LINUX_ENOMEM;
    }

    return (uint64)mapped;
}

/**
 * @brief Linux-compatible mprotect validation wrapper.
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

    if (addr < USER_CODE_VADDR ||
        end > USER_PHDR_VADDR + USER_PHDR_REGION_SIZE)
        return -LINUX_EINVAL;

    return userland_mprotect(addr, length, prot) ? 0 : -LINUX_EINVAL;
}

/**
 * @brief Accept Linux's basic access-pattern hints for valid user mappings.
 *
 * The current VM layer has no page-replacement policy, so these hints do not
 * change mapping behavior.
 */
uint64 sys_madvise(uint64_t addr, uint64_t length, int advice) {
    switch (advice) {
        case LINUX_MADV_NORMAL:
        case LINUX_MADV_RANDOM:
        case LINUX_MADV_SEQUENTIAL:
        case LINUX_MADV_WILLNEED:
            break;
        default:
            return -LINUX_EINVAL;
    }

    if ((addr & 0xFFFULL) != 0)
        return -LINUX_EINVAL;

    uint64_t end = addr + length;
    if (end < addr)
        return -LINUX_EINVAL;

    if (length == 0)
        return 0;

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

    if (in_mmap && !userland_mmap_unmap(addr, length))
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

uint64 sys_statfs(const char *path, linux_statfs_t *buf) {
    if (!path || !buf)
        return -LINUX_EINVAL;

    char norm[256];
    if (!resolve_path_at(LINUX_AT_FDCWD, path, norm, sizeof(norm)))
        return -LINUX_EINVAL;

    vfs_mount_res_t res;
    if (vfs_resolve_mount(norm, &res) != 0)
        return -LINUX_ENOENT;

    if (fill_statfs_for_mount(res.mnt, buf) != 0)
        return -LINUX_EIO;
    if (res.mnt->type == FS_DEV) {
        int device_id = -1;
        uint64_t sectors = 0;
        general_partition_t *part = search_general_partition(res.rel_path);
        if (part) {
            device_id = (int)part->ahci_port;
            sectors = part->sector_count;
        } else {
            for (int i = 0; i < block_device_count; ++i) {
                if (block_devices[i].present &&
                    strcmp(block_devices[i].name, res.rel_path) == 0) {
                    device_id = i;
                    break;
                }
            }
        }
        block_device_info_t *device = block_get_device(device_id);
        if (device && device->type == BLOCK_DEVICE_USB &&
            device->total_sectors == 0 &&
            usb_msc_refresh_device(device->backend_index) != 0)
            return -LINUX_EIO;
        if (device_id >= 0)
            device = block_get_device(device_id);
        if (device && !part)
            sectors = device->total_sectors;
        if (device && device->sector_size != 0 &&
            sectors <= UINT64_MAX / device->sector_size) {
            buf->f_bsize = device->sector_size;
            buf->f_frsize = device->sector_size;
            buf->f_blocks = sectors;
        }
    }
    return 0;
}

uint64 sys_fstatfs(uint64_t fd, linux_statfs_t *buf) {
    if (!buf)
        return -LINUX_EINVAL;
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    vfs_file_t *file = fd_get_file((int)fd);
    if (!file || !file->mnt)
        return -LINUX_EBADF;

    if (fill_statfs_for_mount(file->mnt, buf) != 0)
        return -LINUX_EIO;
    return 0;
}

extern struct memory_context *limine_memory_ctx;
extern uint64_t memory_used;            /* kernel heap bytes in use (from your proc heap file) */

#define SYSINFO_MAX_PID 1024            /* set to your task table size */

uint64_t boot_unix_time;

/* Call once in kernel init, after the RTC is readable. */
void sysinfo_mark_boot(void) {
    boot_unix_time = rtc_get_unix_time();
}

uint64 sys_sysinfo(linux_sysinfo_t *info) {
    if (!info)
        return -LINUX_EFAULT;

    memset(info, 0, sizeof(*info));

    /* uptime: wall clock now minus wall clock at boot */
    uint64_t now = rtc_get_unix_time();
    info->uptime = (now > boot_unix_time) ? (int64_t)(now - boot_unix_time) : 0;

    /* RAM: Linux's totalram is usable RAM, not the whole memory map */
    uint64_t total = limine_memory_ctx->total;
    uint64_t used  = total - limine_memory_ctx->usable;
    info->totalram = total;
    info->freeram  = (used < total) ? total - used : 0;

    /* process count: walk live pids */
    uint16_t procs = 0;
    for (uint32_t pid = 1; pid <= SYSINFO_MAX_PID; pid++) {
        task_info_t t;
        if (multitasking_get_task(pid, &t))
            procs++;
    }
    info->procs = procs;

    /* No page cache, shared memory, swap or highmem exist in FrostWing,
       so these are genuinely zero (already cleared by memset). */
    info->mem_unit = 1;                 /* every size above is in bytes */
    return 0;
}