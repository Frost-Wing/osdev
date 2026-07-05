#include <filesystems/vfs.h>
#include <graphics.h>
#include <memory.h>
#include <net/net.h>
#include <pit.h>
#include <strings.h>
#define IP_TCP 6

extern volatile uint64_t pit_ticks;
#define PIT_MS_PER_TICK 10 // 1000 / pit_freq(100) -- adjust if pit_freq changes

// convert a millisecond duration into a target pit_ticks value
static inline uint64_t ms_to_ticks(uint32 ms) {
    uint64_t t = ms / PIT_MS_PER_TICK;
    return t ? t : 1; // always wait at least 1 tick if any timeout was requested
}

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
    // 400ms timeout for DNS reply, expressed in real time via pit_ticks
    if (udp_recv(49152, &src, &sp, r, &rl, 400) != NET_OK)
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

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_SOCK_UNUSED 0
#define TCP_SOCK_SYN_SENT 1
#define TCP_SOCK_ESTABLISHED 2
#define TCP_SOCK_CLOSING 3

// Increased from 2048 -> 65536 (64 KB) to prevent packet drops during high-throughput
// transfers and allow proper TCP window scaling. The original 2048 was way too small
// relative to the 4096 byte window, causing frequent retransmissions on slow links.
#define TCP_RX_BUF_SIZE 65536

// Maximum TCP payload per segment: 1460 bytes (typical Ethernet MTU minus headers)
// Increased from 1024 to reduce round-trips and improve throughput
#define TCP_MAX_PAYLOAD 1460

struct tcp_sock {
    bool used;
    int state;
    uint16 sport, dport;
    net_ipv4_t dst;
    uint32 seq, ack;
    uint8 rx[TCP_RX_BUF_SIZE];
    size_t rx_len;
    bool fin;
};

static struct tcp_sock tcp_socks[NET_TCP_MAX_SOCKETS];
static uint16 tcp_port = 40000;
static uint32 tcp_iss = 0x10000000;

static uint32 tcp_next_iss(void) {
    tcp_iss += 0x1000;
    return tcp_iss;
}

static uint16 tcp_checksum(net_ipv4_t src, net_ipv4_t dst, const uint8 *seg, size_t len) {
    uint32 sum = 0;
    sum += (src >> 16) & 0xffff;
    sum += src & 0xffff;
    sum += (dst >> 16) & 0xffff;
    sum += dst & 0xffff;
    sum += IP_TCP;
    sum += len;
    for (size_t i = 0; i + 1 < len; i += 2)
        sum += ((uint16)seg[i] << 8) | seg[i + 1];
    if (len & 1)
        sum += ((uint16)seg[len - 1] << 8);
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (uint16)~sum;
}

static int tcp_send_segment(struct tcp_sock *s, uint8 flags, const void *data, size_t len) {
    uint8 b[20 + TCP_MAX_PAYLOAD];
    if (!s || len > TCP_MAX_PAYLOAD)
        return NET_EINVAL;
    struct tcp_hdr *h = (struct tcp_hdr *)b;
    memset(b, 0, sizeof(struct tcp_hdr));
    h->src = net_htons(s->sport);
    h->dst = net_htons(s->dport);
    h->seq = net_htonl(s->seq);
    h->ack = net_htonl(s->ack);
    h->off = 5 << 4;
    h->flags = flags;
    // Advertise our full receive buffer size to allow sender to pump data efficiently
    h->win = net_htons(TCP_RX_BUF_SIZE);
    if (len)
        memcpy(b + sizeof(*h), data, len);
    h->sum = net_htons(tcp_checksum(net_cfg.ip, s->dst, b, sizeof(*h) + len));
    return ipv4_send(s->dst, IP_TCP, b, sizeof(*h) + len);
}

static struct tcp_sock *tcp_by_port(uint16 port) {
    for (int i = 0; i < NET_TCP_MAX_SOCKETS; i++)
        if (tcp_socks[i].used && tcp_socks[i].sport == port)
            return &tcp_socks[i];
    return NULL;
}

int tcp_connect(net_ipv4_t dst, uint16 dport) {
    struct tcp_sock *s = NULL;
    int fd = -1;
    for (int i = 0; i < NET_TCP_MAX_SOCKETS; i++)
        if (!tcp_socks[i].used) {
            s = &tcp_socks[i];
            fd = i;
            break;
        }
    if (!s)
        return NET_ENOMEM;
    memset(s, 0, sizeof(*s));
    s->used = true;
    s->state = TCP_SOCK_SYN_SENT;
    s->sport = tcp_port++;
    s->dport = dport;
    s->dst = dst;
    s->seq = tcp_next_iss();
    if (tcp_send_segment(s, TCP_SYN, NULL, 0) != NET_OK) {
        s->used = false;
        return NET_ERR;
    }
    s->seq++;

    // 5 second timeout for the handshake, measured in real time
    uint64_t deadline = pit_ticks + ms_to_ticks(5000);
    while (pit_ticks < deadline) {
        netif_poll();
        if (s->state == TCP_SOCK_ESTABLISHED)
            return fd;
        asm volatile("hlt");
    }
    s->used = false;
    return NET_ETIMEDOUT;
}

int tcp_send(int sock, const void *data, size_t len) {
    if (sock < 0 || sock >= NET_TCP_MAX_SOCKETS || !tcp_socks[sock].used || tcp_socks[sock].state != TCP_SOCK_ESTABLISHED)
        return NET_ERR;
    struct tcp_sock *s = &tcp_socks[sock];
    const uint8 *p = (const uint8 *)data;
    size_t sent = 0;
    while (sent < len) {
        size_t n = len - sent;
        if (n > TCP_MAX_PAYLOAD)
            n = TCP_MAX_PAYLOAD;
        if (tcp_send_segment(s, TCP_ACK | TCP_PSH, p + sent, n) != NET_OK)
            break;
        s->seq += n;
        sent += n;
    }
    return (int)sent;
}

// `timeout` is now a millisecond duration, measured against real pit_ticks
// instead of a raw spin-loop iteration count.
int tcp_recv(int sock, uint8 *buf, size_t *len, uint32 timeout) {
    if (!len || sock < 0 || sock >= NET_TCP_MAX_SOCKETS || !tcp_socks[sock].used)
        return NET_ERR;
    struct tcp_sock *s = &tcp_socks[sock];

    uint64_t deadline = pit_ticks + ms_to_ticks(timeout);
    while (pit_ticks < deadline) {
        netif_poll();
        if (s->rx_len) {
            size_t n = s->rx_len;
            if (*len < n)
                n = *len;
            memcpy(buf, s->rx, n);
            if (n < s->rx_len)
                memmove(s->rx, s->rx + n, s->rx_len - n);
            s->rx_len -= n;
            *len = n;
            return NET_OK;
        }
        if (s->fin)
            break;
        asm volatile("hlt");
    }
    *len = 0;
    return s->fin ? NET_EOF : NET_ETIMEDOUT;
}

void tcp_close(int sock) {
    if (sock < 0 || sock >= NET_TCP_MAX_SOCKETS || !tcp_socks[sock].used)
        return;
    struct tcp_sock *s = &tcp_socks[sock];
    if (s->state == TCP_SOCK_ESTABLISHED) {
        tcp_send_segment(s, TCP_ACK | TCP_FIN, NULL, 0);
        s->seq++;
    }
    s->used = false;
}

void tcp_input(net_ipv4_t src, const uint8 *p, size_t len) {
    if (len < sizeof(struct tcp_hdr))
        return;
    const struct tcp_hdr *h = (const struct tcp_hdr *)p;
    size_t off = (h->off >> 4) * 4;
    if (off < sizeof(struct tcp_hdr) || off > len)
        return;
    struct tcp_sock *s = tcp_by_port(net_ntohs(h->dst));
    if (!s || s->dst != src || s->dport != net_ntohs(h->src))
        return;
    uint32 seq = net_ntohl(h->seq);
    uint32 ack = net_ntohl(h->ack);
    const uint8 *data = p + off;
    size_t dlen = len - off;
    if (h->flags & TCP_RST) {
        s->used = false;
        return;
    }
    if (s->state == TCP_SOCK_SYN_SENT && (h->flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK) && ack == s->seq) {
        s->ack = seq + 1;
        s->state = TCP_SOCK_ESTABLISHED;
        tcp_send_segment(s, TCP_ACK, NULL, 0);
        return;
    }
    if (s->state != TCP_SOCK_ESTABLISHED)
        return;

    if (dlen && seq == s->ack) {
        size_t room = sizeof(s->rx) - s->rx_len;
        // FIX: previously this always did `s->ack += dlen` even when `room < dlen`,
        // meaning we told the sender "got it all" while actually discarding the
        // tail of the segment that didn't fit. That created a permanent, silent
        // gap in the byte stream with no chance of retransmission.
        //
        // Now: only accept + ack the segment if it fully fits. If it doesn't,
        // drop it entirely without acking, so the remote TCP stack's own
        // retransmit timer will resend it once tcp_recv() has drained rx_len
        // and there's room again.
        if (room >= dlen) {
            memcpy(s->rx + s->rx_len, data, dlen);
            s->rx_len += dlen;
            s->ack += dlen;
            tcp_send_segment(s, TCP_ACK, NULL, 0);
        }
        // else: buffer full, silently drop this segment (no ack sent).
    }
    if (h->flags & TCP_FIN) {
        s->ack = seq + dlen + 1;
        s->fin = true;
        tcp_send_segment(s, TCP_ACK, NULL, 0);
    }
}

static const char *skip_scheme(const char *u) {
    return strncmp(u, "http://", 7) == 0 ? u + 7 : (strncmp(u, "https://", 8) == 0 ? u + 8 : u);
}

int http_get_to_file(const char *url, const char *path,
    wget_progress_cb cb, void *ctx) {
    if (strncmp(url, "https://", 8) == 0) {
        printf("https: TLS is not implemented yet; use http:// fallback\n");
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
    if (s < 0)
        return s;

    char req[512];
    snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", reqpath, host);
    tcp_send(s, req, strlen(req));

    vfs_file_t f;
    if (vfs_open(path, VFS_WRONLY | VFS_CREATE | VFS_TRUNC, &f) != 0) {
        tcp_close(s);
        return NET_ERR;
    }

    // --- parse headers first, so body writing starts clean ---
    char hdrbuf[2048];
    size_t hdrlen = 0;
    uint8 b[TCP_MAX_PAYLOAD];
    int header_done = 0;
    uint64 content_length = 0; // 0 = unknown
    int have_length = 0;
    size_t body_start_in_b = 0;
    size_t body_start_n = 0;

    // consecutive-timeout budget: a single slow gap shouldn't kill the transfer,
    // but a truly dead connection still needs to give up eventually
    int timeouts_in_a_row = 0;
    const int MAX_CONSEC_TIMEOUTS = 20; // ~20 * 3000ms = up to 60s of real stalling

    while (!header_done) {
        size_t n = sizeof(b);
        int r = tcp_recv(s, b, &n, 3000); // 3s per attempt, not 100ms

        if (r == NET_ETIMEDOUT) {
            if (++timeouts_in_a_row > MAX_CONSEC_TIMEOUTS) {
                vfs_close(&f);
                tcp_close(s);
                return NET_ETIMEDOUT;
            }
            continue; // retry, connection is still alive
        }
        if (r != NET_OK || !n) {
            // NET_EOF (clean close) or hard error before headers finished == failure
            vfs_close(&f);
            tcp_close(s);
            return NET_ETIMEDOUT;
        }
        timeouts_in_a_row = 0;

        for (size_t k = 0; k < n; k++) {
            if (hdrlen < sizeof(hdrbuf) - 1)
                hdrbuf[hdrlen++] = (char)b[k];

            if (hdrlen >= 4 &&
                hdrbuf[hdrlen - 4] == '\r' && hdrbuf[hdrlen - 3] == '\n' &&
                hdrbuf[hdrlen - 2] == '\r' && hdrbuf[hdrlen - 1] == '\n') {
                header_done = 1;
                body_start_in_b = k + 1; // remainder of this chunk is body
                body_start_n = n;
                break;
            }
        }
    }
    hdrbuf[hdrlen] = 0;

    // pull Content-Length out of the raw header text (case-sensitive scan is fine for this)
    char *cl = strstr(hdrbuf, "Content-Length:");
    if (!cl)
        cl = strstr(hdrbuf, "content-length:");
    if (cl) {
        cl += 15;
        while (*cl == ' ')
            cl++;
        content_length = 0;
        have_length = 1;
        while (*cl >= '0' && *cl <= '9') {
            content_length = content_length * 10 + (*cl - '0');
            cl++;
        }
    }

    // --- write out whatever body bytes were already in the header chunk ---
    uint64 downloaded = 0;
    if (body_start_in_b < body_start_n) {
        size_t leftover = body_start_n - body_start_in_b;
        vfs_write(&f, b + body_start_in_b, leftover);
        downloaded += leftover;
        if (cb)
            cb(downloaded, content_length, ctx);
    }

    // --- stream the rest of the body ---
    timeouts_in_a_row = 0;
    for (;;) {
        size_t n = sizeof(b);
        int r = tcp_recv(s, b, &n, 3000);

        if (r == NET_OK && n) {
            timeouts_in_a_row = 0;
            vfs_write(&f, b, n);
            downloaded += n;
            if (cb)
                cb(downloaded, have_length ? content_length : 0, ctx);

            // if we already know the total and we've got it all, we're done
            if (have_length && downloaded >= content_length)
                break;
            continue;
        }

        if (r == NET_ETIMEDOUT) {
            if (++timeouts_in_a_row > MAX_CONSEC_TIMEOUTS)
                break; // give up: connection seems dead
            continue;  // transient gap, keep waiting
        }

        // NET_EOF (clean FIN) or hard error -> server is done sending, or died
        break;
    }

    vfs_close(&f);
    tcp_close(s);

    // Don't report success on a partial download: if we knew the expected
    // size and didn't reach it, this was a failure even if some bytes landed.
    if (have_length && downloaded < content_length)
        return NET_ETIMEDOUT;

    return downloaded ? NET_OK : NET_ETIMEDOUT;
}

int dhcp_configure(uint32 timeout_ticks) {
    (void)timeout_ticks;
    return NET_ENOTSUP;
}
