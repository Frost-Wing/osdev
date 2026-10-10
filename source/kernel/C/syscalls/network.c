#include <flanterm/flanterm.h>
#include <syscalls/internal.h>
#include <limine.h>
#include <pit.h>
#include <tty.h>
#include <sys/termios.h>

#define LINUX_AF_INET 2
#define LINUX_SOCK_STREAM 1
#define LINUX_SOCK_DGRAM 2
#define LINUX_SOCK_RAW 3
#define LINUX_SOCK_NONBLOCK 0x800
#define LINUX_SOCK_CLOEXEC 0x80000
#define LINUX_IPPROTO_IP 0
#define LINUX_IPPROTO_ICMP 1
#define LINUX_IPPROTO_TCP 6
#define LINUX_IPPROTO_UDP 17
#define LINUX_IP_RECVTTL 12
#define LINUX_SOL_SOCKET 1
#define LINUX_SO_RCVTIMEO 20
#define LINUX_SO_SNDTIMEO 21
#define LINUX_SO_TYPE 3
#define LINUX_SO_ERROR 4
#define LINUX_SO_BROADCAST 6
#define LINUX_SO_REUSEADDR 2
#define LINUX_IP_TTL 2

#define LINUX_POLLIN 0x001
#define LINUX_POLLOUT 0x004
#define LINUX_POLLHUP 0x010
#define LINUX_POLLNVAL 0x020
#define LINUX_POLLERR 0x008
#define LINUX_MSG_CTRUNC 0x008

#define SOCKET_TIMEOUT_DEFAULT_MS 30000U
#define SOCKET_TIMEOUT_MAX_MS 60000U
#define SOCKET_PORT_FIRST 49153U

typedef struct {
    uint16_t family;
    uint16_t port;
    uint32_t addr;
    uint8_t zero[8];
} linux_sockaddr_in_t;

typedef struct {
    void *name;
    uint32_t namelen;
    uint32_t pad;
    linux_iovec_t *iov;
    uint64_t iovlen;
    void *control;
    uint64_t controllen;
    int32_t flags;
    uint32_t flags_pad;
} linux_msghdr_t;

typedef struct {
    uint16_t family;
    uint8_t data[14];
} linux_sockaddr_t;

typedef struct {
    char name[16];
    union {
        linux_sockaddr_t address;
        int16_t flags;
        int mtu;
        int index;
        uint8_t raw[24];
    } value;
} linux_ifreq_t;

typedef struct {
    int32_t length;
    int32_t padding;
    linux_ifreq_t *buffer;
} linux_ifconf_t;

#define LINUX_SIOCGIFCONF 0x8912
#define LINUX_SIOCGIFFLAGS 0x8913
#define LINUX_SIOCGIFADDR 0x8915
#define LINUX_SIOCGIFDSTADDR 0x8917
#define LINUX_SIOCGIFBRDADDR 0x8919
#define LINUX_SIOCGIFNETMASK 0x891B
#define LINUX_SIOCGIFMTU 0x8921
#define LINUX_SIOCGIFHWADDR 0x8927
#define LINUX_SIOCGIFINDEX 0x8933
#define LINUX_FIONBIO 0x5421

typedef struct {
    bool used;
    int domain;
    int type;
    int protocol;
    bool nonblocking;
    uint64_t fd_bits[STREAM_MAX_FDS / 64];
    bool connected;
    net_ipv4_t peer_addr;
    uint16_t peer_port;
    uint16_t local_port;
    int tcp_id;
    uint32_t recv_timeout_ms;
    uint32_t send_timeout_ms;
    bool recv_ttl;
    uint8_t reply[128];
    size_t reply_len;
    net_ipv4_t reply_addr;
    uint16_t reply_port;
} linux_socket_t;

static linux_socket_t linux_sockets[16];
static uint16_t next_socket_port = SOCKET_PORT_FIRST;

typedef struct {
    int fd;
    short events;
    short revents;
} linux_pollfd_t;

static uint16_t socket_allocate_port(void) {
    uint16_t port = next_socket_port++;
    if (next_socket_port < SOCKET_PORT_FIRST)
        next_socket_port = SOCKET_PORT_FIRST;
    return port;
}

static linux_socket_t *linux_socket_by_fd(int fd) {
    if (fd < 0 || fd >= STREAM_MAX_FDS)
        return NULL;
    for (int i = 0; i < (int)(sizeof(linux_sockets) / sizeof(linux_sockets[0])); i++)
        if (linux_sockets[i].used &&
            (linux_sockets[i].fd_bits[fd / 64] & (1ULL << (fd % 64))))
            return &linux_sockets[i];
    return NULL;
}

static bool socket_has_descriptors(const linux_socket_t *sock) {
    for (size_t i = 0; i < sizeof(sock->fd_bits) / sizeof(sock->fd_bits[0]); i++)
        if (sock->fd_bits[i])
            return true;
    return false;
}

void sys_socket_close(int fd) {
    linux_socket_t *sock = linux_socket_by_fd(fd);
    if (sock) {
        sock->fd_bits[fd / 64] &= ~(1ULL << (fd % 64));
        if (socket_has_descriptors(sock))
            return;
        if (sock->tcp_id >= 0)
            tcp_close(sock->tcp_id);

        if (sock->local_port)
            udp_purge_port(sock->local_port);
        memset(sock, 0, sizeof(*sock));
    }
}

void sys_socket_dup(int oldfd, int newfd) {
    linux_socket_t *sock = linux_socket_by_fd(oldfd);
    if (sock && newfd >= 0 && newfd < STREAM_MAX_FDS)
        sock->fd_bits[newfd / 64] |= 1ULL << (newfd % 64);
}

bool sys_socket_is_fd(int fd) {
    return linux_socket_by_fd(fd) != NULL;
}

static short socket_poll_events(linux_socket_t *sock, short events) {
    short revents = 0;
    if (sock->protocol == LINUX_IPPROTO_UDP) {
        if ((events & LINUX_POLLIN) && udp_has_data(sock->local_port))
            revents |= LINUX_POLLIN;
        if (events & LINUX_POLLOUT)
            revents |= LINUX_POLLOUT;
    } else if (sock->protocol == LINUX_IPPROTO_ICMP) {
        if ((events & LINUX_POLLIN) && sock->reply_len)
            revents |= LINUX_POLLIN;
        if (events & LINUX_POLLOUT)
            revents |= LINUX_POLLOUT;
    } else if (sock->tcp_id >= 0) {
        if ((events & LINUX_POLLIN) &&
            (tcp_has_data(sock->tcp_id) || tcp_is_closed(sock->tcp_id)))
            revents |= LINUX_POLLIN;
        if ((events & LINUX_POLLOUT) && tcp_is_connected(sock->tcp_id))
            revents |= LINUX_POLLOUT;
        if (tcp_is_closed(sock->tcp_id))
            revents |= LINUX_POLLHUP;
    }
    return revents;
}

uint64 sys_poll(void *pollfds, uint64_t nfds, int timeout_ms) {
    if (nfds && !pollfds)
        return -LINUX_EFAULT;
    if (nfds > STREAM_MAX_FDS)
        return -LINUX_EINVAL;

    linux_pollfd_t *fds = (linux_pollfd_t *)pollfds;
    uint64_t deadline = pit_ticks;
    if (timeout_ms > 0)
        deadline += ((uint64_t)timeout_ms + 9) / 10;

    for (;;) {
        if (tty_interrupt_pending())
            return -LINUX_EINTR;
        netif_poll();
        uint64_t ready = 0;
        for (uint64_t i = 0; i < nfds; i++) {
            fds[i].revents = 0;
            if (fds[i].fd < 0)
                continue;

            linux_socket_t *sock = linux_socket_by_fd(fds[i].fd);
            if (sock) {
                fds[i].revents = socket_poll_events(sock, fds[i].events);
            } else if (!fd_valid(fds[i].fd)) {
                fds[i].revents = LINUX_POLLNVAL;
            } else if (fd_is_pipe(fds[i].fd)) {
                /* Only report what was asked for, but POLLERR/POLLHUP are always reported. */
                fds[i].revents = (short)(fd_pipe_poll(fds[i].fd) &
                    (fds[i].events | LINUX_POLLERR | LINUX_POLLHUP));
            } else if (fd_is_eventfd(fds[i].fd)) {
                fds[i].revents = (short)fd_eventfd_poll(fds[i].fd, fds[i].events);
            } else {
                fds[i].revents = fds[i].events;   /* regular files/devices: always ready */
            }
            if (fds[i].revents)
                ready++;
        }
        if (ready || timeout_ms == 0 || (timeout_ms > 0 && pit_ticks >= deadline))
            return ready;
        asm volatile("hlt");
    }
}

uint64 sys_socket(uint64_t domain, uint64_t type, uint64_t protocol) {
    if (domain != LINUX_AF_INET)
        return -LINUX_EAFNOSUPPORT;
    int base_type = (int)(type & 0xFU);
    int proto = (int)protocol;
    if ((type & ~((uint64_t)0xFU | LINUX_SOCK_NONBLOCK |
            LINUX_SOCK_CLOEXEC)) != 0)
        return -LINUX_EINVAL;
    if (base_type == LINUX_SOCK_STREAM) {
        if (proto != 0 && proto != LINUX_IPPROTO_TCP)
            return -LINUX_EPROTONOSUPPORT;
        proto = LINUX_IPPROTO_TCP;
    } else if (base_type == LINUX_SOCK_DGRAM) {
        if (proto != 0 && proto != LINUX_IPPROTO_UDP &&
            proto != LINUX_IPPROTO_ICMP)
            return -LINUX_EPROTONOSUPPORT;
        if (proto == 0)
            proto = LINUX_IPPROTO_UDP;
    } else if (base_type == LINUX_SOCK_RAW) {
        if (proto != LINUX_IPPROTO_ICMP)
            return -LINUX_EPROTONOSUPPORT;
    } else {
        return -LINUX_EPROTONOSUPPORT;
    }

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
    sock->fd_bits[fd / 64] |= 1ULL << (fd % 64);
    sock->domain = (int)domain;
    sock->type = base_type;
    sock->protocol = proto;
    sock->nonblocking = (type & LINUX_SOCK_NONBLOCK) != 0;
    sock->tcp_id = -1;
    sock->recv_timeout_ms = SOCKET_TIMEOUT_DEFAULT_MS;
    sock->send_timeout_ms = SOCKET_TIMEOUT_DEFAULT_MS;
    return fd;
}

uint64 sys_connect(uint64_t fd, const void *addr, uint64_t addrlen) {
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return fd_valid((int)fd) ? -LINUX_ENOTSOCK : -LINUX_EBADF;
    if (!addr || addrlen < sizeof(linux_sockaddr_in_t))
        return -LINUX_EINVAL;

    const linux_sockaddr_in_t *in = (const linux_sockaddr_in_t *)addr;
    if (in->family != LINUX_AF_INET)
        return -LINUX_EAFNOSUPPORT;
    sock->peer_addr = net_ntohl(in->addr);
    sock->peer_port = net_ntohs(in->port);
    sock->connected = true;

    if (sock->type == LINUX_SOCK_STREAM) {
        int tcp_id = tcp_connect(sock->peer_addr, sock->peer_port);
        if (tcp_id == NET_ETIMEDOUT) {
            sock->connected = false;
            return -LINUX_ETIMEDOUT;
        }
        if (tcp_id < 0) {
            sock->connected = false;
            return -LINUX_ECONNREFUSED;
        }
        sock->tcp_id = tcp_id;
    }
    if (sock->type == LINUX_SOCK_DGRAM && sock->protocol == LINUX_IPPROTO_UDP &&
        sock->local_port == 0)
        sock->local_port = socket_allocate_port();
    return 0;
}

uint64 sys_getsockname(uint64_t fd, void *addr, uint64_t *addrlen) {
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return fd_valid((int)fd) ? -LINUX_ENOTSOCK : -LINUX_EBADF;
    if (!addr || !addrlen)
        return -LINUX_EFAULT;
    if (*addrlen < sizeof(linux_sockaddr_in_t))
        return -LINUX_EINVAL;

    linux_sockaddr_in_t *in = (linux_sockaddr_in_t *)addr;
    memset(in, 0, sizeof(*in));
    in->family = LINUX_AF_INET;
    in->port = net_htons(sock->local_port);
    in->addr = net_htonl(net_cfg.ip);
    *addrlen = sizeof(*in);
    return 0;
}

uint64 sys_bind(uint64_t fd, const void *addr, uint64_t addrlen) {
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return fd_valid((int)fd) ? -LINUX_ENOTSOCK : -LINUX_EBADF;
    if (!addr || addrlen < sizeof(linux_sockaddr_in_t))
        return -LINUX_EINVAL;

    const linux_sockaddr_in_t *in = (const linux_sockaddr_in_t *)addr;
    if (in->family != LINUX_AF_INET)
        return -LINUX_EAFNOSUPPORT;
    if (in->addr != 0 && net_ntohl(in->addr) != net_cfg.ip)
        return -LINUX_EINVAL;
    if (sock->type == LINUX_SOCK_STREAM)
        return in->port == 0 ? 0 : -LINUX_EOPNOTSUPP;

    uint16_t port = net_ntohs(in->port);
    if (port == 0)
        port = socket_allocate_port();
    for (int i = 0; i < (int)(sizeof(linux_sockets) / sizeof(linux_sockets[0])); i++) {
        linux_socket_t *other = &linux_sockets[i];
        if (other != sock && other->used && other->local_port == port)
            return -LINUX_EADDRINUSE;
    }
    sock->local_port = port;
    return 0;
}

uint64 sys_sendto(uint64_t fd, const void *buf, uint64_t len, uint64_t flags,
    const void *addr, uint64_t addrlen) {
    (void)flags;
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return fd_valid((int)fd) ? -LINUX_ENOTSOCK : -LINUX_EBADF;
    if (!buf && len)
        return -LINUX_EINVAL;

    linux_sockaddr_in_t destination;
    if (addr) {
        if (addrlen < sizeof(destination))
            return -LINUX_EINVAL;
        memcpy(&destination, addr, sizeof(destination));
        if (destination.family != LINUX_AF_INET)
            return -LINUX_EAFNOSUPPORT;
        sock->peer_addr = net_ntohl(destination.addr);
        sock->peer_port = net_ntohs(destination.port);
        sock->connected = true;
    } else if (!sock->connected) {
        return -LINUX_EDESTADDRREQ;
    }

    if (sock->type == LINUX_SOCK_STREAM) {
        if (sock->tcp_id < 0)
            return -LINUX_ENOTCONN;
        int sent = tcp_send(sock->tcp_id, buf, len);
        return sent < 0 ? -LINUX_EPIPE : (uint64)sent;
    }

    if (sock->protocol == LINUX_IPPROTO_UDP) {
        if (!sock->local_port)
            sock->local_port = socket_allocate_port();
        int rc = udp_send(sock->peer_addr, sock->local_port, sock->peer_port, buf, len);
        return rc == NET_OK ? len : -LINUX_ENETUNREACH;
    }

    uint16_t id = 0x4242;
    uint16_t seq = 0;
    if (len >= 8) {
        const uint8_t *icmp = (const uint8_t *)buf;
        id = ((uint16_t)icmp[4] << 8) | icmp[5];
        seq = ((uint16_t)icmp[6] << 8) | icmp[7];
    }

    int r = icmp_ping(sock->peer_addr, id, seq, 1000);
    if (r != NET_OK)
        return r == NET_ETIMEDOUT ? -LINUX_ETIMEDOUT : -LINUX_ENETUNREACH;

    sock->reply_addr = sock->peer_addr;
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
    if (!buf && len)
        return -LINUX_EINVAL;
    size_t n;
    net_ipv4_t src_addr = sock->reply_addr;
    uint16_t src_port = sock->reply_port;
    if (sock->type == LINUX_SOCK_STREAM) {
        if (sock->tcp_id < 0)
            return -LINUX_ENOTCONN;
        n = len;
        int rc = tcp_recv(sock->tcp_id, buf, &n,
            sock->nonblocking ? 0 : sock->recv_timeout_ms);
        if (rc == NET_EOF)
            return 0;
        if (rc == NET_EINTR)
            return -LINUX_EINTR;
        if (rc == NET_ECONNRESET)
            return -LINUX_ECONNRESET;
        if (rc == NET_ETIMEDOUT)
            return sock->nonblocking ? -LINUX_EAGAIN : -LINUX_ETIMEDOUT;
        if (rc == NET_EINVAL)
            return -LINUX_EINVAL;
        if (rc != NET_OK)
            return -LINUX_EIO;
        src_addr = sock->peer_addr;
        src_port = sock->peer_port;
    } else if (sock->protocol == LINUX_IPPROTO_UDP) {
        n = len;
        int rc = udp_recv(sock->local_port, &src_addr, &src_port, buf, &n,
            sock->nonblocking ? 0 : sock->recv_timeout_ms);
        if (rc == NET_ETIMEDOUT)
            return sock->nonblocking ? -LINUX_EAGAIN : -LINUX_ETIMEDOUT;
        if (rc != NET_OK)
            return -LINUX_EIO;
    } else {
        if (!sock->reply_len)
            return -LINUX_EAGAIN;
        if (sock->type == LINUX_SOCK_RAW) {
            struct {
                uint8_t version_ihl;
                uint8_t tos;
                uint16_t total_len;
                uint16_t id;
                uint16_t fragment;
                uint8_t ttl;
                uint8_t protocol;
                uint16_t checksum;
                uint32_t source;
                uint32_t destination;
            } __attribute__((packed)) ip_header;
            uint8_t packet[sizeof(ip_header) + sizeof(sock->reply)];
            size_t packet_len = sizeof(ip_header) + sock->reply_len;

            memset(&ip_header, 0, sizeof(ip_header));
            ip_header.version_ihl = 0x45;
            ip_header.total_len = net_htons((uint16_t)packet_len);
            ip_header.ttl = 64;
            ip_header.protocol = LINUX_IPPROTO_ICMP;
            ip_header.source = net_htonl(src_addr);
            ip_header.destination = net_htonl(net_cfg.ip);
            ip_header.checksum = net_htons(net_checksum(&ip_header, sizeof(ip_header)));
            memcpy(packet, &ip_header, sizeof(ip_header));
            memcpy(packet + sizeof(ip_header), sock->reply, sock->reply_len);
            n = packet_len < len ? packet_len : len;
            memcpy(buf, packet, n);
        } else {
            n = sock->reply_len < len ? sock->reply_len : len;
            memcpy(buf, sock->reply, n);
        }
        sock->reply_len = 0;
    }

    if (addr && addrlen) {
        if (*addrlen < sizeof(linux_sockaddr_in_t))
            return -LINUX_EINVAL;
        linux_sockaddr_in_t *in = (linux_sockaddr_in_t *)addr;
        memset(in, 0, sizeof(*in));
        in->family = LINUX_AF_INET;
        in->port = net_htons(src_port);
        in->addr = net_htonl(src_addr);
        *addrlen = sizeof(*in);
    }
    return n;
}

uint64 sys_recvmsg(uint64_t fd, void *message, uint64_t flags) {
    (void)flags;
    if (!message)
        return -LINUX_EFAULT;

    linux_msghdr_t *msg = (linux_msghdr_t *)message;
    if (msg->iovlen > 16 || (msg->iovlen && !msg->iov))
        return -LINUX_EINVAL;
    uint64_t control_capacity = msg->controllen;

    uint8_t data[NET_MTU];
    uint64_t capacity = 0;
    for (uint64_t i = 0; i < msg->iovlen; ++i) {
        if (!msg->iov[i].iov_base && msg->iov[i].iov_len)
            return -LINUX_EFAULT;
        if (msg->iov[i].iov_len > sizeof(data) - capacity)
            capacity = sizeof(data);
        else
            capacity += msg->iov[i].iov_len;
    }

    linux_sockaddr_in_t source;
    uint64_t source_len = sizeof(source);
    uint64_t received = sys_recvfrom(fd, data, capacity, 0,
        msg->name ? &source : NULL, msg->name ? &source_len : NULL);
    if ((int64_t)received < 0)
        return received;

    if (msg->name) {
        size_t copy_len = msg->namelen < sizeof(source) ?
            msg->namelen : sizeof(source);
        memcpy(msg->name, &source, copy_len);
        msg->namelen = sizeof(source);
    } else {
        msg->namelen = 0;
    }

    uint64_t remaining = received;
    size_t offset = 0;
    for (uint64_t i = 0; i < msg->iovlen && remaining; ++i) {
        uint64_t chunk = msg->iov[i].iov_len < remaining ?
            msg->iov[i].iov_len : remaining;
        memcpy((void*)msg->iov[i].iov_base, data + offset, chunk);
        offset += (size_t)chunk;
        remaining -= chunk;
    }
    msg->controllen = 0;
    msg->flags = 0;
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (sock && sock->recv_ttl) {
        struct {
            uint64_t length;
            int32_t level;
            int32_t type;
            int32_t ttl;
        } __attribute__((packed)) cmsg = {
            .length = sizeof(cmsg),
            .level = LINUX_IPPROTO_IP,
            .type = LINUX_IP_TTL,
            .ttl = 64,
        };
        const uint64_t cmsg_space = 24;
        if (msg->control && control_capacity >= sizeof(cmsg)) {
            memcpy(msg->control, &cmsg, sizeof(cmsg));
            msg->controllen = control_capacity < cmsg_space ?
                sizeof(cmsg) : cmsg_space;
        } else if (msg->control || control_capacity) {
            msg->flags |= LINUX_MSG_CTRUNC;
        }
    }
    return received;
}

uint64 sys_setsockopt(uint64_t fd, uint64_t level, uint64_t optname,
    const void *optval, uint64_t optlen) {
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return fd_valid((int)fd) ? -LINUX_ENOTSOCK : -LINUX_EBADF;
    if (level == LINUX_SOL_SOCKET &&
        (optname == LINUX_SO_RCVTIMEO || optname == LINUX_SO_SNDTIMEO)) {
        if (!optval || optlen < sizeof(int64_t) * 2)
            return -LINUX_EINVAL;
        const int64_t *tv = (const int64_t *)optval;
        if (tv[0] < 0 || tv[1] < 0 || tv[1] >= 1000000)
            return -LINUX_EINVAL;
        uint64_t ms = (uint64_t)tv[0] * 1000ULL +
                      ((uint64_t)tv[1] + 999ULL) / 1000ULL;
        if (ms == 0)
            ms = SOCKET_TIMEOUT_MAX_MS;
        if (ms > SOCKET_TIMEOUT_MAX_MS)
            ms = SOCKET_TIMEOUT_MAX_MS;
        if (optname == LINUX_SO_RCVTIMEO)
            sock->recv_timeout_ms = (uint32_t)ms;
        else
            sock->send_timeout_ms = (uint32_t)ms;
        return 0;
    }
    if (level == LINUX_SOL_SOCKET &&
        (optname == LINUX_SO_BROADCAST || optname == LINUX_SO_REUSEADDR))
        return optval && optlen >= sizeof(int) ? 0 : -LINUX_EINVAL;
    if (level == LINUX_SOL_SOCKET &&
        (optname == 5 || optname == 7 || optname == 8 || optname == 9 || optname == 10))
        return optval && optlen >= sizeof(int) ? 0 : -LINUX_EINVAL;
    if (level == LINUX_IPPROTO_IP && optname == LINUX_IP_TTL)
        return optval && optlen >= sizeof(int) ? 0 : -LINUX_EINVAL;
    if (level == LINUX_IPPROTO_IP && optname == LINUX_IP_RECVTTL) {
        if (!optval || optlen < sizeof(int))
            return -LINUX_EINVAL;
        sock->recv_ttl = *(const int *)optval != 0;
        return 0;
    }
    if (level == LINUX_IPPROTO_IP && (optname == 1 || optname == 3 ||
        optname == 8 || optname == 11 || optname == 12))
        return optval && optlen >= sizeof(int) ? 0 : -LINUX_EINVAL;
    return -LINUX_ENOPROTOOPT;
}

uint64 sys_getsockopt(uint64_t fd, uint64_t level, uint64_t optname,
    void *optval, uint64_t *optlen) {
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return fd_valid((int)fd) ? -LINUX_ENOTSOCK : -LINUX_EBADF;
    if (!optval || !optlen)
        return -LINUX_EFAULT;

    int value;
    if (level == LINUX_SOL_SOCKET && optname == LINUX_SO_TYPE)
        value = sock->type;
    else if (level == LINUX_SOL_SOCKET && optname == LINUX_SO_ERROR)
        value = 0;
    else if (level == LINUX_IPPROTO_IP && optname == LINUX_IP_TTL)
        value = 64;
    else if (level == LINUX_IPPROTO_IP && optname == LINUX_IP_RECVTTL)
        value = sock->recv_ttl ? 1 : 0;
    else if (level == LINUX_SOL_SOCKET &&
        (optname == LINUX_SO_RCVTIMEO || optname == LINUX_SO_SNDTIMEO)) {
        if (*optlen < sizeof(int64_t) * 2)
            return -LINUX_EINVAL;
        uint32_t ms = optname == LINUX_SO_RCVTIMEO ?
            sock->recv_timeout_ms : sock->send_timeout_ms;
        int64_t timeval[2] = { ms / 1000U, (ms % 1000U) * 1000U };
        memcpy(optval, timeval, sizeof(timeval));
        *optlen = sizeof(timeval);
        return 0;
    }
    else
        return -LINUX_ENOPROTOOPT;

    if (*optlen < sizeof(value))
        return -LINUX_EINVAL;
    memcpy(optval, &value, sizeof(value));
    *optlen = sizeof(value);
    return 0;
}

uint64 sys_socket_read(uint64_t fd, void *buf, uint64_t len) {
    return sys_recvfrom(fd, buf, len, 0, NULL, NULL);
}

uint64 sys_socket_write(uint64_t fd, const void *buf, uint64_t len) {
    linux_socket_t *sock = linux_socket_by_fd((int)fd);
    if (!sock)
        return -LINUX_ENOTSOCK;
    if (!sock->connected)
        return -LINUX_ENOTCONN;
    return sys_sendto(fd, buf, len, 0, NULL, 0);
}

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

static bool ioctl_get_block_device(vfs_file_t *file, int fd, block_device_info_t **device,
    uint64_t *start_lba, uint64_t *sector_count, bool *read_only) {
    *start_lba = 0;
    *sector_count = 0;
    *read_only = false;
    const char *device_name = NULL;
    if (file && file->mnt && file->mnt->type == FS_DEV)
        device_name = file->rel_path;
    if (!device_name) {
        const char *path = fd_get_path(fd);
        if (!path || strncmp(path, "/dev/", 5) != 0)
            return false;
        device_name = path + 5;
    }
    for (int i = 0; i < block_device_count; i++) {
        block_device_info_t *dev = &block_devices[i];
        if (dev->present && strcmp(dev->name, device_name) == 0) {
            if (dev->type == BLOCK_DEVICE_USB && dev->total_sectors == 0) {
                if (usb_msc_refresh_device(dev->backend_index) != 0)
                    return false;
                dev = block_get_device(i);
                if (!dev)
                    return false;
            }
            *device = dev;
            *sector_count = dev->total_sectors;
            return true;
        }
    }

    for (int i = 0; i < general_partition_count; i++) {
        general_partition_t *part = &ahci_partitions[i];
        if (strcmp(part->name, device_name) != 0)
            continue;
        block_device_info_t *dev = block_get_device((int)part->ahci_port);
        if (!dev)
            return false;
        if (dev->type == BLOCK_DEVICE_USB && dev->total_sectors == 0) {
            if (usb_msc_refresh_device(dev->backend_index) != 0)
                return false;
            dev = block_get_device((int)part->ahci_port);
            if (!dev)
                return false;
        }
        *device = dev;
        *start_lba = part->lba_start;
        *sector_count = part->sector_count;
        *read_only = part->fs_type == FS_ISO9660;
        return true;
    }
    return false;
}

static bool ioctl_path_matches(vfs_file_t *file, int fd, const char *name) {
    if (file && file->mnt && file->mnt->type == FS_DEV &&
        strcmp(file->rel_path, name) == 0)
        return true;
    const char *path = fd_get_path(fd);
    return path && strcmp(path, name) == 0;
}

static bool ioctl_fd_is_tty(vfs_file_t *file, int fd) {
    if (sys_socket_is_fd(fd))
        return false;
    return fd_is_tty(fd) ||
        ioctl_path_matches(file, fd, "/dev/tty") ||
        ioctl_path_matches(file, fd, "tty") ||
        ioctl_path_matches(file, fd, "/dev/tty1") ||
        ioctl_path_matches(file, fd, "tty1");
}

static uint64 ioctl_rtc_read_time(vfs_file_t *file, int fd, uint64_t arg) {
    if (!ioctl_path_matches(file, fd, "rtc") &&
        !ioctl_path_matches(file, fd, "rtc0") &&
        !ioctl_path_matches(file, fd, "/dev/rtc") &&
        !ioctl_path_matches(file, fd, "/dev/rtc0"))
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

static bool ioctl_if_name_valid(const char name[16], bool *loopback) {
    if (strcmp(name, "lo") == 0) {
        *loopback = true;
        return true;
    }
    if (strcmp(name, "eth0") == 0) {
        *loopback = false;
        return true;
    }
    return false;
}

static void ioctl_set_if_address(linux_sockaddr_t *address, net_ipv4_t ip) {
    memset(address, 0, sizeof(*address));
    address->family = LINUX_AF_INET;
    uint32_t network_ip = net_htonl(ip);
    memcpy(address->data + 2, &network_ip, sizeof(network_ip));
}

static uint64 ioctl_network_request(int fd, uint64_t req, uint64_t arg) {
    if (!sys_socket_is_fd(fd))
        return -LINUX_ENOTTY;
    if (req != LINUX_SIOCGIFCONF && req != LINUX_SIOCGIFFLAGS &&
        req != LINUX_SIOCGIFADDR && req != LINUX_SIOCGIFDSTADDR &&
        req != LINUX_SIOCGIFBRDADDR && req != LINUX_SIOCGIFNETMASK &&
        req != LINUX_SIOCGIFMTU && req != LINUX_SIOCGIFHWADDR &&
        req != LINUX_SIOCGIFINDEX)
        return -LINUX_ENOTTY;
    if (!arg)
        return -LINUX_EFAULT;

    if (req == LINUX_SIOCGIFCONF) {
        linux_ifconf_t *conf = (linux_ifconf_t *)arg;
        if (conf->length < 0)
            return -LINUX_EINVAL;
        if (!conf->buffer || conf->length < (int32_t)sizeof(linux_ifreq_t)) {
            conf->length = 0;
            return 0;
        }
        int32_t count = conf->length / (int32_t)sizeof(linux_ifreq_t);
        if (count > 2)
            count = 2;
        memset(conf->buffer, 0, (size_t)count * sizeof(linux_ifreq_t));
        if (count > 0) {
            strcpy(conf->buffer[0].name, "lo");
            ioctl_set_if_address(&conf->buffer[0].value.address,
                net_ipv4_from_octets(127, 0, 0, 1));
        }
        if (count > 1) {
            strcpy(conf->buffer[1].name, "eth0");
            ioctl_set_if_address(&conf->buffer[1].value.address, net_cfg.ip);
        }
        conf->length = count * (int32_t)sizeof(linux_ifreq_t);
        return 0;
    }

    linux_ifreq_t *request = (linux_ifreq_t *)arg;
    request->name[sizeof(request->name) - 1] = '\0';
    bool loopback;
    if (!ioctl_if_name_valid(request->name, &loopback))
        return -LINUX_ENODEV;

    switch (req) {
        case LINUX_SIOCGIFFLAGS:
            request->value.flags = loopback ? 0x0009 : 0x1043;
            return 0;
        case LINUX_SIOCGIFADDR:
            ioctl_set_if_address(&request->value.address,
                loopback ? net_ipv4_from_octets(127, 0, 0, 1) : net_cfg.ip);
            return 0;
        case LINUX_SIOCGIFNETMASK:
            ioctl_set_if_address(&request->value.address,
                loopback ? net_ipv4_from_octets(255, 0, 0, 0) : net_cfg.netmask);
            return 0;
        case LINUX_SIOCGIFDSTADDR:
            ioctl_set_if_address(&request->value.address,
                loopback ? net_ipv4_from_octets(127, 0, 0, 1) : net_cfg.gateway);
            return 0;
        case LINUX_SIOCGIFBRDADDR:
            ioctl_set_if_address(&request->value.address,
                loopback ? net_ipv4_from_octets(127, 255, 255, 255) :
                (net_cfg.ip | ~net_cfg.netmask));
            return 0;
        case LINUX_SIOCGIFMTU:
            request->value.mtu = loopback ? 65536 : NET_MTU;
            return 0;
        case LINUX_SIOCGIFINDEX:
            request->value.index = loopback ? 1 : 2;
            return 0;
        case LINUX_SIOCGIFHWADDR:
            memset(&request->value.address, 0, sizeof(request->value.address));
            request->value.address.family = loopback ? 772 : 1;
            if (!loopback)
                memcpy(request->value.address.data, net_cfg.mac, sizeof(net_cfg.mac));
            return 0;
        default:
            return -LINUX_ENOTTY;
    }
}

static uint64 ioctl_tty_termios(vfs_file_t *file, int fd,
    uint64_t req, uint64_t arg) {
    if (!ioctl_fd_is_tty(file, fd))
        return -LINUX_ENOTTY;
    if (!arg)
        return -LINUX_EINVAL;

    if (req == LINUX_TCGETS2) {
        linux_termios_t current;
        if (!tty_get_termios(&current))
            return -LINUX_EIO;

        linux_termios2_t *termios = (linux_termios2_t *)arg;
        termios->c_iflag = current.c_iflag;
        termios->c_oflag = current.c_oflag;
        termios->c_cflag = current.c_cflag;
        termios->c_lflag = current.c_lflag;
        termios->c_line = current.c_line;
        memcpy(termios->c_cc, current.c_cc, sizeof(termios->c_cc));
        termios->c_ispeed = 38400;
        termios->c_ospeed = 38400;
        return 0;
    }

    linux_termios2_t *termios = (linux_termios2_t *)arg;
    linux_termios_t updated = {
        .c_iflag = termios->c_iflag,
        .c_oflag = termios->c_oflag,
        .c_cflag = termios->c_cflag,
        .c_lflag = termios->c_lflag,
        .c_line = termios->c_line,
    };
    memcpy(updated.c_cc, termios->c_cc, sizeof(updated.c_cc));
    bool flush_input = req == LINUX_TCSETSF2;
    return tty_set_termios(&updated, flush_input) ? 0 : -LINUX_EIO;
}

uint64 sys_ioctl(uint64_t fd, uint64_t req, uint64_t arg) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    vfs_file_t *file = fd_get_file((int)fd);
    uint64 network_result = ioctl_network_request((int)fd, req, arg);
    if (network_result != (uint64)-LINUX_ENOTTY)
        return network_result;

    if (req == LINUX_RTC_RD_TIME)
        return ioctl_rtc_read_time(file, (int)fd, arg);

    if (req == LINUX_TCGETS2 || req == LINUX_TCSETS2 ||
        req == LINUX_TCSETSW2 || req == LINUX_TCSETSF2)
        return ioctl_tty_termios(file, (int)fd, req, arg);

    if (req == LINUX_BLKGETSIZE64 || req == LINUX_BLKGETSIZE ||
        req == LINUX_BLKSSZGET || req == LINUX_BLKROGET) {
        block_device_info_t *device = NULL;
        uint64_t start_lba, sectors;
        bool read_only;
        if (!ioctl_get_block_device(file, (int)fd, &device, &start_lba, &sectors, &read_only))
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

    if (sys_socket_is_fd((int)fd) && req == LINUX_FIONBIO) {
        linux_socket_t *s = linux_socket_by_fd((int)fd);
        if (!s)
            return -LINUX_EBADF;
        if (!arg)
            return -LINUX_EINVAL;
        s->nonblocking = *(int *)arg != 0;
        return 0;
    }

    switch (req) {
        case LINUX_TIOCGWINSZ: {
            if (!ioctl_fd_is_tty(file, (int)fd))
                return -LINUX_ENOTTY;
            linux_winsize_t *ws = (linux_winsize_t *)arg;
            if (!ws)
                return -LINUX_EINVAL;
            return tty_get_winsize(ws) ? 0 : -LINUX_EIO;
        }
        case LINUX_TCGETS: {
            if (!ioctl_fd_is_tty(file, (int)fd))
                return -LINUX_ENOTTY;
            linux_termios_t *tio = (linux_termios_t *)arg;
            if (!tio)
                return -LINUX_EINVAL;

            return tty_get_termios(tio) ? 0 : -LINUX_EIO;
        }
        case LINUX_TCSETS:
        case LINUX_TCSETSW:
        case LINUX_TCSETSF: {
            if (!ioctl_fd_is_tty(file, (int)fd))
                return -LINUX_ENOTTY;
            if (!arg)
                return -LINUX_EINVAL;
            return tty_set_termios((const linux_termios_t *)arg,
                req == LINUX_TCSETSF) ? 0 : -LINUX_EIO;
        }
        case LINUX_TIOCGPGRP:
            if (!ioctl_fd_is_tty(file, (int)fd))
                return -LINUX_ENOTTY;
            if (!arg)
                return -LINUX_EINVAL;
            *(int *)arg = (int)multitasking_current_pid();
            return 0;
        case LINUX_TIOCSPGRP:
            if (!ioctl_fd_is_tty(file, (int)fd))
                return -LINUX_ENOTTY;
            return arg ? 0 : -LINUX_EINVAL;
        default:
            return -LINUX_ENOTTY;
    }
}

uint64 sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg) {
    if (!fd_valid((int)fd))
        return -LINUX_EBADF;

    switch (cmd) {
        case LINUX_F_DUPFD:
        case LINUX_F_DUPFD_CLOEXEC: {
            if (arg >= STREAM_MAX_FDS)
                return -LINUX_EINVAL;
            for (int target = (int)arg; target < STREAM_MAX_FDS; ++target) {
                if (!fd_valid(target)) {
                    int duplicated = fd_dup2((int)fd, target);
                    if (duplicated >= 0)
                        sys_socket_dup((int)fd, duplicated);
                    return duplicated;
                }
            }
            return -LINUX_ENFILE;
        }
        case LINUX_F_GETFD:
        case LINUX_F_SETFD:
            return 0;
        case LINUX_F_SETFL: {
            linux_socket_t *s = linux_socket_by_fd((int)fd);
            if (s) s->nonblocking = (arg & 0x800) != 0;
            return 0;
        }
        case LINUX_F_GETFL: {
            linux_socket_t *s = linux_socket_by_fd((int)fd);
            return fd_flags((int)fd) | (s && s->nonblocking ? LINUX_SOCK_NONBLOCK : 0);
        }
        default:
            return -LINUX_ENOSYS;
    }
}
