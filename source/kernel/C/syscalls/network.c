#include <syscalls/internal.h>


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

uint64 sys_ioctl(uint64_t fd, uint64_t req, uint64_t arg) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    switch (req) {
        case LINUX_TIOCGWINSZ: {
            linux_winsize_t *ws = (linux_winsize_t *)arg;
            if (!ws)
                return -LINUX_EINVAL;
            ws->ws_row = 25;
            ws->ws_col = 80;
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
