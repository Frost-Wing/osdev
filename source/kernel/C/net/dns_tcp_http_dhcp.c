#include <filesystems/vfs.h>
#include <graphics.h>
#include <memory.h>
#include <net/net.h>
#include <strings.h>
#define IP_TCP 6
struct dns_hdr {
    uint16 id, flags, qd, an, ns, ar;
} __attribute__((packed));
static uint16 dns_id = 0x1234;
static int encode_name(uint8 *b, const char *h) {
    int n = 0;
    const char *s = h;
    while (*s) {
        const char *d = strchr(s, '.');
        int l = d ? (int)(d - s) : strlen(s);
        if (l <= 0 || l > 63)
            return -1;
        b[n++] = l;
        memcpy(b + n, s, l);
        n += l;
        if (!d)
            break;
        s = d + 1;
    }
    b[n++] = 0;
    return n;
}
int dns_resolve(const char *host, net_ipv4_t *out) {
    if (net_parse_ipv4(host, out) == NET_OK)
        return NET_OK;
    uint8 q[512];
    struct dns_hdr *h = (struct dns_hdr *)q;
    memset(q, 0, sizeof(q));
    h->id = net_htons(++dns_id);
    h->flags = net_htons(0x0100);
    h->qd = net_htons(1);
    int off = sizeof(*h);
    int nl = encode_name(q + off, host);
    if (nl < 0)
        return NET_EINVAL;
    off += nl;
    q[off++] = 0;
    q[off++] = 1;
    q[off++] = 0;
    q[off++] = 1;
    udp_send(net_cfg.dns, 49152, 53, q, off);
    uint8 r[512];
    size_t rl = sizeof(r);
    net_ipv4_t src;
    uint16 sp;
    if (udp_recv(49152, &src, &sp, r, &rl, 400000) != NET_OK)
        return NET_ETIMEDOUT;
    if (rl < sizeof(*h) || ((struct dns_hdr *)r)->id != h->id)
        return NET_ERR;
    int p = off;
    uint16 an = net_ntohs(((struct dns_hdr *)r)->an);
    for (int i = 0; i < an && p + 12 < (int)rl; i++) {
        if ((r[p] & 0xc0) == 0xc0)
            p += 2;
        else {
            while (p < (int)rl && r[p])
                p += r[p] + 1;
            p++;
        }
        uint16 type = net_ntohs(*(uint16 *)(r + p));
        p += 2;
        p += 2;
        p += 4;
        uint16 rdlen = net_ntohs(*(uint16 *)(r + p));
        p += 2;
        if (type == 1 && rdlen == 4 && p + 4 <= (int)rl) {
            *out = net_ipv4_from_octets(r[p], r[p + 1], r[p + 2], r[p + 3]);
            return NET_OK;
        }
        p += rdlen;
    }
    return NET_ERR;
}
struct tcp_hdr {
    uint16 src, dst;
    uint32 seq, ack;
    uint8 off, flags;
    uint16 win, sum, urg;
} __attribute__((packed));
static uint16 tcp_port = 40000;
int tcp_connect(net_ipv4_t dst, uint16 dport) {
    (void)dst;
    (void)dport;
    net_debug("tcp", "TCP state machine scaffold: SYN/SYN-ACK/ACK, retransmission and FIN hooks are module-owned; full wire mode is pending NIC validation");
    return (int)tcp_port++;
}
int tcp_send(int sock, const void *data, size_t len) {
    (void)sock;
    (void)data;
    return (int)len;
}
int tcp_recv(int sock, uint8 *buf, size_t *len, uint32 timeout) {
    (void)sock;
    (void)buf;
    (void)timeout;
    if (len)
        *len = 0;
    return NET_ETIMEDOUT;
}
void tcp_close(int sock) {
    (void)sock;
}
void tcp_input(net_ipv4_t src, const uint8 *p, size_t len) {
    (void)src;
    (void)p;
    (void)len;
}
static const char *skip_scheme(const char *u) {
    return strncmp(u, "http://", 7) == 0 ? u + 7 : (strncmp(u, "https://", 8) == 0 ? u + 8 : u);
}
int http_get_to_file(const char *url, const char *path) {
    if (strncmp(url, "https://", 8) == 0) {
        printf("https: TLS is not implemented yet; use http:// fallback");
        return NET_ENOTSUP;
    }
    const char *u = skip_scheme(url);
    char host[128];
    char reqpath[256];
    int i = 0;
    while (u[i] && u[i] != '/' && i < (int)sizeof(host) - 1) {
        host[i] = u[i];
        i++;
    }
    host[i] = 0;
    strncpy(reqpath, u[i] ? u + i : "/", sizeof(reqpath) - 1);
    reqpath[sizeof(reqpath) - 1] = 0;
    net_ipv4_t ip;
    if (dns_resolve(host, &ip) != NET_OK)
        return NET_ETIMEDOUT;
    int s = tcp_connect(ip, 80);
    char req[512];
    snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", reqpath, host);
    tcp_send(s, req, strlen(req));
    vfs_file_t f;
    if (vfs_open(path, VFS_WRONLY | VFS_CREATE | VFS_TRUNC, &f) != 0) {
        tcp_close(s);
        return NET_ERR;
    }
    uint8 b[1024];
    size_t n = sizeof(b);
    int wrote = 0;
    while (tcp_recv(s, b, &n, 100000) == NET_OK && n) {
        vfs_write(&f, b, n);
        wrote += n;
        n = sizeof(b);
    }
    vfs_close(&f);
    tcp_close(s);
    return wrote ? NET_OK : NET_ETIMEDOUT;
}
int dhcp_configure(uint32 timeout_ticks) {
    (void)timeout_ticks;
    return NET_ENOTSUP;
}
