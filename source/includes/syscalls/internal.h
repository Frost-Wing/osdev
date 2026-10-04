#ifndef KERNEL_SYSCALLS_INTERNAL_H
#define KERNEL_SYSCALLS_INTERNAL_H

#include <commands/login.h>
#include <graphics.h>
#include <keyboard.h>
#include <klog.h>
#include <limine.h>
#include <memory.h>
#include <stdint.h>
#include <stream.h>
#include <strings.h>
#include <syscalls.h>
#include <syslog.h>
#include <userland.h>
#include <debugger.h>
#include <filesystems/ext2.h>
#include <filesystems/fat16.h>
#include <filesystems/fat32.h>
#include <filesystems/iso9660.h>
#include <filesystems/layers/dev.h>
#include <filesystems/layers/proc.h>
#include <filesystems/vfs.h>
#include <ahci.h>
#include <cc-asm.h>
#include <executables/elf.h>
#include <heap.h>
#include <multitasking.h>
#include <net/net.h>
#include <rtc.h>
#include <tty.h>
#include <sys/dirent.h>
#include <sys/helper.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/utsname.h>

extern struct limine_framebuffer *framebuffer;
extern uint64 *font_address;
extern void reboot(void);
extern void shutdown(void);
extern uint64_t current_fs_base;
extern uint32_t current_umask;
extern uint64_t *clear_child_tid;

typedef struct {
    bool exists;
    bool is_dir;
    uint64_t size;
    uint32_t mode;
    uint64_t inode;
    uint64_t rdev;
    linux_timespec_t atim;
    linux_timespec_t mtim;
    linux_timespec_t ctim;
} vfs_stat_info_t;

typedef struct {
    int64_t  uptime;
    uint64_t loads[3];
    uint64_t totalram;
    uint64_t freeram;
    uint64_t sharedram;
    uint64_t bufferram;
    uint64_t totalswap;
    uint64_t freeswap;
    uint16_t procs;
    uint16_t pad;
    uint32_t pad2;
    uint64_t totalhigh;
    uint64_t freehigh;
    uint32_t mem_unit;
    uint8_t  _f[4];
} linux_sysinfo_t;

_Static_assert(sizeof(linux_sysinfo_t) == 112, "sysinfo ABI size");

/* Cross-module helpers and implementations used by the central dispatcher. */
bool resolve_path_at(int dirfd, const char *path, char *out, size_t out_sz);
bool fill_vfs_stat_for_path_at(int dirfd, const char *path, vfs_stat_info_t *info);
bool fill_vfs_lstat_for_path_at(int dirfd, const char *path, vfs_stat_info_t *info);
uint64 sys_set_file_times(const char *path, uint64_t atime, uint64_t mtime);
uint64 copy_readlink_result(const char *target, char *buf, uint64_t bufsiz);
bool path_is_loadable_elf(const char *path);
bool should_route_to_toybox(const char *target);
int build_toybox_argv(const char *target, int argc, char **argv, const char **out_argv);
void sys_socket_close(int fd);
void sys_socket_dup(int oldfd, int newfd);
bool sys_socket_is_fd(int fd);
uint64 sys_socket_read(uint64_t fd, void *buf, uint64_t len);
uint64 sys_socket_write(uint64_t fd, const void *buf, uint64_t len);

uint64 sys_mkdirat(int, const char *, int); uint64_t sys_unlink(const char *); uint64 sys_unlinkat(int, const char *, int); int sys_getdents64(uint64_t, char *, uint64_t);
uint64 sys_open_common(int, const char *, int, int); uint64 sys_close(uint64_t);
uint64 sys_fstat(uint64_t, linux_stat_t *); uint64 sys_stat(const char *, linux_stat_t *);
uint64 sys_newfstatat(int, const char *, linux_stat_t *, int); uint64 sys_statx(int, const char *, int, unsigned int, linux_statx_t *);
uint64 sys_read(uint64_t, char *, uint64_t); uint64 sys_write(uint64_t, const char *, uint64_t); uint64 sys_writev(uint64_t, const linux_iovec_t *, uint64_t);
uint64 sys_socket(uint64_t, uint64_t, uint64_t); uint64 sys_connect(uint64_t, const void *, uint64_t);
uint64 sys_poll(void *, uint64_t, int);
uint64 sys_getsockname(uint64_t, void *, uint64_t *);
uint64 sys_recvmsg(uint64_t, void *, uint64_t);
uint64 sys_bind(uint64_t, const void *, uint64_t);
uint64 sys_sendto(uint64_t, const void *, uint64_t, uint64_t, const void *, uint64_t); uint64 sys_recvfrom(uint64_t, void *, uint64_t, uint64_t, void *, uint64_t *); uint64 sys_setsockopt(uint64_t, uint64_t, uint64_t, const void *, uint64_t);
uint64 sys_getsockopt(uint64_t, uint64_t, uint64_t, void *, uint64_t *);
uint64 sys_ioctl(uint64_t, uint64_t, uint64_t); uint64 sys_fcntl(uint64_t, uint64_t, uint64_t);
uint64 sys_access_common(int, const char *, int); uint64 sys_lseek(uint64_t, int64_t, uint64_t); uint64 sys_dup2(uint64_t, uint64_t); uint64 sys_dup3(uint64_t, uint64_t, uint64_t); uint64 sys_dup(uint64_t); uint64 sys_getcwd(char *, uint64_t); uint64 sys_chdir(const char *); uint64 sys_readlinkat(int, const char *, char *, uint64_t);
uint64 sys_clock_gettime(uint64_t, linux_timespec_t *); uint64 sys_nanosleep(const linux_timespec_t *, linux_timespec_t *);
uint64 sys_clock_nanosleep(uint64_t, uint64_t, const linux_timespec_t *, linux_timespec_t *);
uint64 sys_pipe2(int *, uint64_t);
uint64 sys_fchdir(uint64_t);
uint64 sys_gettimeofday(linux_timeval_t *, linux_timezone_t *);
uint64 sys_pselect6(int, uint64_t *, uint64_t *, uint64_t *,
    const linux_timespec_t *, const void *);
uint64 sys_time(int64_t *tloc);
uint64 sys_utimes(const char *, const void *);
uint64 sys_futimesat(int, const char *, const void *);
uint64 sys_utimensat(int, const char *, const linux_timespec_t *, int);
int sys_reboot(int, int, unsigned int, void *); int sys_kill(int, int); uint64 sys_syslog(int, char *, uint64_t);
uint64 sys_mmap(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t); uint64 sys_mprotect(uint64_t, uint64_t, uint64_t); uint64 sys_madvise(uint64_t, uint64_t, int); uint64 sys_brk(uint64_t); uint64 sys_munmap(uint64_t, uint64_t); uint64 sys_arch_prctl(uint64_t, uint64_t); uint64 sys_prlimit64(uint64_t, uint64_t, const linux_rlimit64_t *, linux_rlimit64_t *); uint64 sys_umask(uint64_t); uint64 sys_tgkill(uint64_t, uint64_t, uint64_t); uint64 sys_set_tid_address(uint64_t *); uint64 sys_set_robust_list(const void *, uint64_t); uint64 sys_getrandom(void *, uint64_t, uint64_t);
uint64 sys_rseq(void *, uint64_t, uint64_t, uint32_t);
void sys_rseq_reset_current(void);
uint64 sys_rt_sigaction(int, const task_signal_action_t *, task_signal_action_t *, uint64_t);
uint64 sys_execve(const char *, char *const *, char *const *); uint64 sys_fork(void); uint64 sys_clone3(const void *, uint64_t); uint64 sys_wait4(int64_t, int *, int, void *); uint64 sys_futex(uint32_t *, int, uint32_t, const linux_timespec_t *, uint32_t *, uint32_t);
int fill_statfs_for_mount(mount_entry_t *mnt, linux_statfs_t *out);
uint64 sys_statfs(const char *path, linux_statfs_t *buf);
uint64 sys_fstatfs(uint64_t fd, linux_statfs_t *buf);
uint64 sys_rename(const char *oldpath, const char *newpath);
uint64 sys_copy_file_range(uint64_t in_fd, int64_t *off_in, uint64_t out_fd,
    int64_t *off_out, uint64_t len, uint64_t flags);
uint64 sys_mount(const char *source, const char *target, const char *filesystem,
    uint64_t flags, const void *data);
uint64 sys_umount2(const char *target, int flags);
void sysinfo_mark_boot(void);
uint64 sys_sysinfo(linux_sysinfo_t *info);
uint64 sys_eventfd2(uint64_t initial_value, uint64_t flags);
uint64 sys_ftruncate(uint64_t fd, int64_t length);

#endif
