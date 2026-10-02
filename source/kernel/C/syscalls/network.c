#include <flanterm/flanterm.h>
#include <syscalls/internal.h>
#include <limine.h>

#define LINUX_AF_INET 2
#define LINUX_SOCK_RAW 3
#define LINUX_IPPROTO_ICMP 1

typedef struct {
    uint16_t family;
    uint16_t port;
    uint32_t addr;
    uint8_t zero[8];
} linux_sockaddr_in_t;

typedef struct {
    bool used;
    int fd;
    int domain;
    int type;
    int protocol;
    uint8_t reply[128];
    size_t reply_len;
    net_ipv4_t reply_addr;
} linux_socket_t;

static linux_socket_t linux_sockets[16];

static linux_socket_t *linux_socket_by_fd(int fd) {
    for (int i = 0; i < (int)(sizeof(linux_sockets) / sizeof(linux_sockets[0])); i++)
        if (linux_sockets[i].used && linux_sockets[i].fd == fd)
            return &linux_sockets[i];
    return NULL;
}

void sys_socket_close(int fd) {
    linux_socket_t *sock = linux_socket_by_fd(fd);
    if (sock)
        memset(sock, 0, sizeof(*sock));
}

uint64 sys_socket(uint64_t domain, uint64_t type, uint64_t protocol) {
    if (domain != LINUX_AF_INET)
        return -LINUX_EAFNOSUPPORT;
    if (type != LINUX_SOCK_RAW || protocol != LINUX_IPPROTO_ICMP)
        return -LINUX_EPROTONOSUPPORT;

    linux_socket_t *sock = NULL;
    for (int i = 0; i < (int)(sizeof(linux_sockets) / sizeof(linux_sockets[0])); i++) {
        if (!linux_sockets[i].used) {
            sock = &linux_sockets[i];
            break;
        }
    }
    if (!sock)
        return -LINUX_ENFILE;

    int fd = fd_create_virtual("/dev/socket/icmp", VFS_RDWR);
    if (fd < 0)
        return -LINUX_ENFILE;

    memset(sock, 0, sizeof(*sock));
    sock->used = true;
    sock->fd = fd;
    sock->domain = (int)domain;
    sock->type = (int)type;
    sock->protocol = (int)protocol;
    return fd;
}

uint64 sys_connect(uint64_t fd, const void *addr, uint64_t addrlen) {
    (void)addr;
    (void)addrlen;

    if (!fd_valid((int)fd))
        return -LINUX_EBADF;
    return -LINUX_ENOTSOCK;
}

uint64 sys_sendto(uint64_t fd, const void *buf, uint64_t len, uint64_t flags,
    const void *addr, uint64_t addrlen) {
    (void)flags;
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return fd_valid((int)fd) ? -LINUX_ENOTSOCK : -LINUX_EBADF;
    if (!buf || !addr || addrlen < sizeof(linux_sockaddr_in_t))
        return -LINUX_EINVAL;

    const linux_sockaddr_in_t *in = (const linux_sockaddr_in_t *)addr;
    if (in->family != LINUX_AF_INET)
        return -LINUX_EAFNOSUPPORT;

    net_ipv4_t dst = net_ntohl(in->addr);
    uint16_t id = 0x4242;
    uint16_t seq = 0;
    if (len >= 8) {
        const uint8_t *icmp = (const uint8_t *)buf;
        id = ((uint16_t)icmp[4] << 8) | icmp[5];
        seq = ((uint16_t)icmp[6] << 8) | icmp[7];
    }

    int r = icmp_ping(dst, id, seq, 500000);
    if (r != NET_OK)
        return -LINUX_ETIMEDOUT;

    sock->reply_addr = dst;
    sock->reply_len = len < sizeof(sock->reply) ? len : sizeof(sock->reply);
    memcpy(sock->reply, buf, sock->reply_len);
    if (sock->reply_len >= 1)
        sock->reply[0] = 0; /* echo reply */
    if (sock->reply_len >= 4) {
        sock->reply[2] = 0;
        sock->reply[3] = 0;
        uint16_t sum = net_checksum(sock->reply, sock->reply_len);
        sock->reply[2] = (uint8_t)(sum >> 8);
        sock->reply[3] = (uint8_t)sum;
    }
    return len;
}

uint64 sys_recvfrom(uint64_t fd, void *buf, uint64_t len, uint64_t flags,
    void *addr, uint64_t *addrlen) {
    (void)flags;
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return fd_valid((int)fd) ? -LINUX_ENOTSOCK : -LINUX_EBADF;
    if (!buf)
        return -LINUX_EINVAL;
    if (!sock->reply_len)
        return -LINUX_EAGAIN;

    size_t n = sock->reply_len < len ? sock->reply_len : len;
    memcpy(buf, sock->reply, n);
    sock->reply_len = 0;

    if (addr && addrlen && *addrlen >= sizeof(linux_sockaddr_in_t)) {
        linux_sockaddr_in_t *in = (linux_sockaddr_in_t *)addr;
        memset(in, 0, sizeof(*in));
        in->family = LINUX_AF_INET;
        in->addr = net_htonl(sock->reply_addr);
        *addrlen = sizeof(*in);
    }
    return n;
}

uint64 sys_setsockopt(uint64_t fd, uint64_t level, uint64_t optname,
    const void *optval, uint64_t optlen) {
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return fd_valid((int)fd) ? -LINUX_ENOTSOCK : -LINUX_EBADF;
    return 0;
}

extern struct flanterm_context *ft_ctx;

typedef struct {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
} linux_rtc_time_t;

static bool ioctl_get_block_device(vfs_file_t *file, block_device_info_t **device,
    uint64_t *start_lba, uint64_t *sector_count, bool *read_only) {
    if (!file || !file->mnt || file->mnt->type != FS_DEV)
        return false;

    *start_lba = 0;
    *sector_count = 0;
    *read_only = false;
    for (int i = 0; i < block_device_count; i++) {
        block_device_info_t *dev = &block_devices[i];
        if (dev->present && strcmp(dev->name, file->rel_path) == 0) {
            *device = dev;
            *sector_count = dev->total_sectors;
            return true;
        }
    }

    for (int i = 0; i < general_partition_count; i++) {
        general_partition_t *part = &ahci_partitions[i];
        if (strcmp(part->name, file->rel_path) != 0)
            continue;
        block_device_info_t *dev = block_get_device((int)part->ahci_port);
        if (!dev)
            return false;
        *device = dev;
        *start_lba = part->lba_start;
        *sector_count = part->sector_count;
        *read_only = part->fs_type == FS_ISO9660;
        return true;
    }
    return false;
}

static uint64 ioctl_rtc_read_time(vfs_file_t *file, uint64_t arg) {
    if (!file || !file->mnt || file->mnt->type != FS_DEV ||
        (strcmp(file->rel_path, "rtc") != 0 &&
         strcmp(file->rel_path, "rtc0") != 0))
        return -LINUX_ENOTTY;
    if (!arg)
        return -LINUX_EINVAL;

    uint8_t sec, min, hour, day, month;
    uint16_t year;
    update_system_time(&sec, &min, &hour, &day, &month, &year);
    if (month < 1 || month > 12 || day < 1 || day > 31)
        return -LINUX_EIO;

    linux_rtc_time_t *rtc = (linux_rtc_time_t *)arg;
    memset(rtc, 0, sizeof(*rtc));
    rtc->tm_sec = sec;
    rtc->tm_min = min;
    rtc->tm_hour = hour;
    rtc->tm_mday = day;
    rtc->tm_mon = (int)month - 1;
    rtc->tm_year = (int)year - 1900;
    rtc->tm_wday = (int)((rtc_get_unix_time() / 86400ULL + 4ULL) % 7ULL);
    for (uint8_t m = 1; m < month; m++) {
        static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        rtc->tm_yday += days[m - 1];
        if (m == 2 && (year % 4 == 0) &&
            ((year % 100 != 0) || (year % 400 == 0)))
            rtc->tm_yday++;
    }
    rtc->tm_yday += day - 1;
    rtc->tm_isdst = 0;
    return 0;
}

uint64 sys_ioctl(uint64_t fd, uint64_t req, uint64_t arg) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    vfs_file_t *file = fd_get_file((int)fd);
    if (req == LINUX_RTC_RD_TIME)
        return ioctl_rtc_read_time(file, arg);

    if (req == LINUX_BLKGETSIZE64 || req == LINUX_BLKGETSIZE ||
        req == LINUX_BLKSSZGET || req == LINUX_BLKROGET) {
        block_device_info_t *device = NULL;
        uint64_t start_lba, sectors;
        bool read_only;
        if (!ioctl_get_block_device(file, &device, &start_lba, &sectors, &read_only))
            return -LINUX_ENOTTY;
        (void)start_lba;
        if (!arg)
            return -LINUX_EINVAL;
        if (device->sector_size == 0 ||
            sectors > UINT64_MAX / device->sector_size)
            return -LINUX_EIO;

        switch (req) {
            case LINUX_BLKGETSIZE64:
                *(uint64_t *)arg = sectors * device->sector_size;
                break;
            case LINUX_BLKGETSIZE:
                *(uint64_t *)arg = (sectors * device->sector_size) / 512U;
                break;
            case LINUX_BLKSSZGET:
                *(int *)arg = (int)device->sector_size;
                break;
            case LINUX_BLKROGET:
                *(int *)arg = read_only ? 1 : 0;
                break;
        }
        return 0;
    }

    switch (req) {
        case LINUX_TIOCGWINSZ: {
            linux_winsize_t *ws = (linux_winsize_t *)arg;
            if (!ws)
                return -LINUX_EINVAL;
            ws->ws_row = ft_ctx->rows;
            ws->ws_col = ft_ctx->cols;
            ws->ws_xpixel = 0;
            ws->ws_ypixel = 0;
            return 0;
        }
        case LINUX_TCGETS: {
            linux_termios_t *tio = (linux_termios_t *)arg;
            if (!tio)
                return -LINUX_EINVAL;

            memset(tio, 0, sizeof(*tio));

            /* Input flags */
            tio->c_iflag =
                LINUX_ICRNL | // CR -> NL
                LINUX_IXON;   // Ctrl-S/Ctrl-Q flow control

            /* Output flags */
            tio->c_oflag =
                LINUX_OPOST | // enable output processing
                LINUX_ONLCR;  // NL -> CRNL

            /* Control flags */
            tio->c_cflag =
                LINUX_CREAD |
                LINUX_CS8;

            /* Local flags */
            tio->c_lflag =
                LINUX_ISIG |
                LINUX_ICANON |
                LINUX_ECHO |
                LINUX_ECHOE |
                LINUX_ECHOK |
                LINUX_IEXTEN;

            /* Special characters */
            tio->c_cc[LINUX_VMIN] = 1;
            tio->c_cc[LINUX_VTIME] = 0;

            return 0;
        }
        case LINUX_TCSETS:
        case LINUX_TCSETSW:
        case LINUX_TCSETSF:
            return arg ? 0 : -LINUX_EINVAL;
        case LINUX_TIOCGPGRP:
            if (!arg)
                return -LINUX_EINVAL;
            *(int *)arg = 1;
            return 0;
        case LINUX_TIOCSPGRP:
            return arg ? 0 : -LINUX_EINVAL;
        default:
            return -LINUX_ENOTTY;
    }
}

uint64 sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    switch (cmd) {
        case LINUX_F_DUPFD: {
            if (arg >= STREAM_MAX_FDS)
                return -LINUX_EINVAL;
            for (int target = (int)arg; target < STREAM_MAX_FDS; ++target) {
                if (!fd_valid(target))
                    return fd_dup2((int)fd, target);
            }
            return -LINUX_ENFILE;
        }
        case LINUX_F_GETFD:
        case LINUX_F_SETFD:
            return 0;
        case LINUX_F_GETFL:
            return fd_flags((int)fd);
        case LINUX_F_SETFL:
            return 0;
        default:
            return -LINUX_ENOSYS;
    }
}
