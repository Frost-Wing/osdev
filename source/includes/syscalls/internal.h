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
} vfs_stat_info_t;

/* Cross-module helpers and implementations used by the central dispatcher. */
bool resolve_path_at(int dirfd, const char *path, char *out, size_t out_sz);
bool fill_vfs_stat_for_path_at(int dirfd, const char *path, vfs_stat_info_t *info);
uint64 copy_readlink_result(const char *target, char *buf, uint64_t bufsiz);
bool path_is_loadable_elf(const char *path);
bool should_route_to_toybox(const char *target);
int build_toybox_argv(const char *target, int argc, char **argv, const char **out_argv);
void sys_socket_close(int fd);

uint64 sys_mkdirat(int, const char *, int); uint64_t sys_unlink(const char *); int sys_getdents64(uint64_t, char *, uint64_t);
uint64 sys_open_common(int, const char *, int, int); uint64 sys_close(uint64_t);
uint64 sys_fstat(uint64_t, linux_stat_t *); uint64 sys_stat(const char *, linux_stat_t *);
uint64 sys_newfstatat(int, const char *, linux_stat_t *, int); uint64 sys_statx(int, const char *, int, unsigned int, linux_statx_t *);
uint64 sys_read(uint64_t, char *, uint64_t); uint64 sys_write(uint64_t, const char *, uint64_t); uint64 sys_writev(uint64_t, const linux_iovec_t *, uint64_t);
uint64 sys_socket(uint64_t, uint64_t, uint64_t); uint64 sys_connect(uint64_t, const void *, uint64_t);
uint64 sys_sendto(uint64_t, const void *, uint64_t, uint64_t, const void *, uint64_t); uint64 sys_recvfrom(uint64_t, void *, uint64_t, uint64_t, void *, uint64_t *); uint64 sys_setsockopt(uint64_t, uint64_t, uint64_t, const void *, uint64_t);
uint64 sys_ioctl(uint64_t, uint64_t, uint64_t); uint64 sys_fcntl(uint64_t, uint64_t, uint64_t);
uint64 sys_access_common(int, const char *, int); uint64 sys_lseek(uint64_t, int64_t, uint64_t); uint64 sys_dup2(uint64_t, uint64_t); uint64 sys_dup(uint64_t); uint64 sys_getcwd(char *, uint64_t); uint64 sys_chdir(const char *); uint64 sys_readlinkat(int, const char *, char *, uint64_t);
uint64 sys_clock_gettime(uint64_t, linux_timespec_t *); uint64 sys_nanosleep(const linux_timespec_t *, linux_timespec_t *);
int sys_reboot(int, int, unsigned int, void *); int sys_kill(int, int); uint64 sys_syslog(int, char *, uint64_t);
uint64 sys_mmap(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t); uint64 sys_mprotect(uint64_t, uint64_t, uint64_t); uint64 sys_brk(uint64_t); uint64 sys_munmap(uint64_t, uint64_t); uint64 sys_arch_prctl(uint64_t, uint64_t); uint64 sys_prlimit64(uint64_t, uint64_t, const linux_rlimit64_t *, linux_rlimit64_t *); uint64 sys_umask(uint64_t); uint64 sys_tgkill(uint64_t, uint64_t, uint64_t); uint64 sys_set_tid_address(uint64_t *); uint64 sys_set_robust_list(const void *, uint64_t); uint64 sys_getrandom(void *, uint64_t, uint64_t);
uint64 sys_execve(const char *, char *const *, char *const *); uint64 sys_fork(void); uint64 sys_wait4(int64_t, int *, int, void *); uint64 sys_futex(uint32_t *, int, uint32_t, const linux_timespec_t *, uint32_t *, uint32_t);

#endif
