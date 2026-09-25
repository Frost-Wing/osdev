#include <syscalls/internal.h>
#include <syscalls/sysnames.h>

void int80_handler(InterruptFrame *frame) {
    uint64_t ret = syscall_dispatch(
        frame->rax,
        frame->rdi,
        frame->rsi,
        frame->rdx,
        frame->rcx, // arg4 (int 0x80 uses rcx)
        frame->r8,
        frame->r9);

    frame->rax = ret;
}

// THIS IS FOR SYSCALL INSTRUCTION
void syscall_handler(syscall_frame_t *f) {
    if (f && (f->rax == LINUX_SYS_EXIT || f->rax == LINUX_SYS_EXIT_GROUP)) {
        /*
         * set_tid_address(2)'s clear_child_tid points into the exiting
         * userspace image. This kernel does not have copy_to_user/futex
         * teardown yet, and directly dereferencing it from ring 0 can fault
         * during process exit (for example after toybox sh runs a child and
         * exits). Treat it as bookkeeping only for now rather than allowing a
         * best-effort compatibility write to panic the kernel.
         */
        clear_child_tid = NULL;
        uint32_t pid = multitasking_current_pid();
        if (pid)
            multitasking_exit_task(pid, (int)f->rdi);
        if (userland_prepare_exit(f, f->rdi))
            return;
    }

    uint64_t ret = syscall_dispatch(
        f->rax,
        f->rdi,
        f->rsi,
        f->rdx,
        f->r10, // ⚠️ DIFFERENT HERE
        f->r8,
        f->r9);

    f->rax = ret;
}

uint64_t syscall_dispatch(
    uint64_t nr,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5,
    uint64_t arg6) {

    const char *syscall_name = (nr < (sizeof(names) / sizeof(names[0])) && names[nr]) ? names[nr] : "?";
    syslog_printf("[syscall] %s(%u)", syscall_name, nr);
    debug_printf("[syscall] %s(%u)\n", syscall_name, nr);

    switch (nr) {
        case LINUX_SYS_READ:
            return sys_read(arg1, (char *)arg2, arg3);

        case LINUX_SYS_WRITE:
            return sys_write(arg1, (const char *)arg2, arg3);

        case LINUX_SYS_OPEN:
            return sys_open_common(LINUX_AT_FDCWD, (const char *)arg1, arg2, arg3);

        case LINUX_SYS_FSTAT:
            return sys_fstat(arg1, (linux_stat_t *)arg2);

        case LINUX_SYS_STAT:
            return sys_stat((const char *)arg1, (linux_stat_t *)arg2);

        case LINUX_SYS_LSTAT:
            return sys_newfstatat(
                LINUX_AT_FDCWD,
                (const char *)arg1,
                (linux_stat_t *)arg2,
                LINUX_AT_SYMLINK_NOFOLLOW);

        case LINUX_SYS_MPROTECT:
            return sys_mprotect(arg1, arg2, arg3);

        case LINUX_SYS_RT_SIGACTION:
            return 0;

        case LINUX_SYS_RT_SIGPROCMASK:
        case LINUX_SYS_SIGALTSTACK:
            return -LINUX_ENOSYS;

        case LINUX_SYS_ACCESS:
            return sys_access_common(LINUX_AT_FDCWD, (const char *)arg1, arg2);

        case LINUX_SYS_OPENAT:
            return sys_open_common((int)arg1, (const char *)arg2, arg3, arg4);

        case LINUX_SYS_CLOSE:
            return sys_close(arg1);

        case LINUX_SYS_LSEEK:
            return sys_lseek(arg1, (int64_t)arg2, arg3);

        case LINUX_SYS_MMAP:
            return sys_mmap(arg1, arg2, arg3, arg4, arg5, arg6);

        case LINUX_SYS_MUNMAP:
            return sys_munmap(arg1, arg2);

        case LINUX_SYS_BRK:
            return sys_brk(arg1);

        case LINUX_SYS_IOCTL:
            return sys_ioctl(arg1, arg2, arg3);

        case LINUX_SYS_WRITEV:
            return sys_writev(arg1, (linux_iovec_t *)arg2, arg3);

        case LINUX_SYS_DUP:
            return sys_dup(arg1);

        case LINUX_SYS_DUP2:
            return sys_dup2(arg1, arg2);

        case LINUX_SYS_NANOSLEEP:
            return sys_nanosleep((const linux_timespec_t *)arg1, (linux_timespec_t *)arg2);

        case LINUX_SYS_SCHED_YIELD:
            multitasking_yield();
            return 0;

        case LINUX_SYS_GETPID:
            return multitasking_current_pid() ? multitasking_current_pid() : 1;

        case LINUX_SYS_SOCKET:
            return sys_socket(arg1, arg2, arg3);

        case LINUX_SYS_CONNECT:
            return sys_connect(arg1, (const void *)arg2, arg3);

        case LINUX_SYS_SENDTO:
            return sys_sendto(arg1, (const void *)arg2, arg3, arg4, (const void *)arg5, arg6);

        case LINUX_SYS_RECVFROM:
            return sys_recvfrom(arg1, (void *)arg2, arg3, arg4, (void *)arg5, (uint64_t *)arg6);

        case LINUX_SYS_SETSOCKOPT:
            return sys_setsockopt(arg1, arg2, arg3, (const void *)arg4, arg5);

        case LINUX_SYS_CLONE: {
            /*
             * musl implements fork()/vfork() on x86_64 with clone(SIGCHLD, 0)
             * instead of the obsolete fork syscall. Treat that exact no-shared-
             * address-space form as fork so toybox sh can spawn applets. Real
             * thread-like clone flags still require process context cloning and
             * are intentionally rejected.
             */
            const uint64_t LINUX_SIGCHLD = 17;
            if ((arg1 & ~0xFFULL) != 0 || (arg1 & 0xFFULL) != LINUX_SIGCHLD ||
                arg2 != 0 || arg3 != 0 || arg4 != 0 || arg5 != 0)
                return -LINUX_ENOSYS;
            return sys_fork();
        }

        case LINUX_SYS_EXECVE:
            return sys_execve((const char *)arg1, (char *const *)arg2, (char *const *)arg3);

        case LINUX_SYS_EXIT:
        case LINUX_SYS_EXIT_GROUP:
            return 0;

        case LINUX_SYS_GETCWD:
            return sys_getcwd((char *)arg1, arg2);

        case LINUX_SYS_FCNTL:
            return sys_fcntl(arg1, arg2, arg3);

        case LINUX_SYS_CHDIR:
            return sys_chdir((const char *)arg1);

        case LINUX_SYS_FORK:
        case LINUX_SYS_VFORK:
            return sys_fork();

        case LINUX_SYS_UNAME:
            return sys_uname((linux_utsname_t *)arg1);

        case LINUX_SYS_READLINK:
            return sys_readlinkat(LINUX_AT_FDCWD, (const char *)arg1, (char *)arg2, arg3);

        case LINUX_SYS_UMASK:
            return sys_umask(arg1);

        case LINUX_SYS_GETUID:
        case LINUX_SYS_GETEUID:
        case LINUX_SYS_GETGID:
        case LINUX_SYS_GETEGID:
            return 0;

        case LINUX_SYS_GETPPID: {
            task_info_t info = {0};
            uint32_t pid = multitasking_current_pid();
            if (pid && multitasking_get_task(pid, &info))
                return info.parent_pid ? info.parent_pid : 1;
            return 1;
        }

        case LINUX_SYS_WAIT4:
            return sys_wait4((int64_t)arg1, (int *)arg2, (int)arg3, (void *)arg4);

        case LINUX_SYS_ARCH_PRCTL:
            return sys_arch_prctl(arg1, arg2);

        case LINUX_SYS_GETTID:
            return multitasking_current_pid() ? multitasking_current_pid() : 1;

        case LINUX_SYS_TGKILL:
            return sys_tgkill(arg1, arg2, arg3);

        case LINUX_SYS_GETDENTS64:
            return sys_getdents64(arg1, (char *)arg2, arg3);

        case LINUX_SYS_SET_TID_ADDRESS:
            return sys_set_tid_address((uint64_t *)arg1);

        case LINUX_SYS_CLOCK_GETTIME:
            return sys_clock_gettime(arg1, (linux_timespec_t *)arg2);

        case LINUX_SYS_NEWFSTATAT:
            return sys_newfstatat((int)arg1, (const char *)arg2, (linux_stat_t *)arg3, (int)arg4);

        case LINUX_SYS_READLINKAT:
            return sys_readlinkat((int)arg1, (const char *)arg2, (char *)arg3, arg4);

        case LINUX_SYS_FACCESSAT:
            return sys_access_common((int)arg1, (const char *)arg2, (int)arg3);

        case LINUX_SYS_SET_ROBUST_LIST:
            return sys_set_robust_list((const void *)arg1, arg2);

        case LINUX_SYS_PRLIMIT64:
            return sys_prlimit64(arg1, arg2, (const linux_rlimit64_t *)arg3, (linux_rlimit64_t *)arg4);

        case LINUX_SYS_GETRANDOM:
            return sys_getrandom((void *)arg1, arg2, arg3);

        case LINUX_SYS_STATX:
            return sys_statx((int)arg1, (const char *)arg2, (int)arg3, (unsigned int)arg4, (linux_statx_t *)arg5);

        case LINUX_SYS_SYNC: // sync
            return vfs_sync(false);

        case LINUX_SYS_KILL:
            return sys_kill((int)arg1, (int)arg2);

        case LINUX_SYS_REBOOT:
            return sys_reboot(
                (int)arg1,
                (int)arg2,
                (unsigned int)arg3,
                (void *)arg4);
            return 1;

        case LINUX_SYS_SYSLOG: // syslog, this is what dmesg calls
            return sys_syslog((int)arg1, (char *)arg2, arg3);

        case LINUX_SYS_MKDIR:
            return sys_mkdirat(LINUX_AT_FDCWD, (const char *)arg1, arg2);

        case LINUX_SYS_MKDIRAT:
            return sys_mkdirat((int)arg1, (const char *)arg2, arg3);

        case LINUX_SYS_UNLINK:
            return sys_unlink((const char *)arg1);

        case PRAD_MAGIC:
            info("Alive from userland", __FILE__);
            return 0;

        case LINUX_SYS_FUTEX:
            return sys_futex((uint32_t *)arg1, arg2, arg3,
                (const linux_timespec_t *)arg4,
                (uint32_t *)arg5, arg6);
        case 19: {
            linux_iovec_t *iov = (linux_iovec_t *)arg2;
            if (arg3 > 0)
                return sys_read(arg1, (char*)iov[0].iov_base, iov[0].iov_len);
            return 0;
        }

        case 7: {
            struct pollfd {
                int fd;
                short events;
                short revents;
            };

            struct pollfd *fds = (struct pollfd *)arg1;

            for (int i = 0; i < arg2; i++) {
                fds[i].revents = fds[i].events; // pretend ready
            }

            return arg2;
        }
        default:
            printf(linux_syscalls_prefix "Unknown, returning -ENOSYS for (%u)", nr);
            return -LINUX_ENOSYS;
    }
}
