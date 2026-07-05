#include <graphics.h>
#include <memory.h>
#include <net/net.h>
#include <strings.h>
#define ETH_ARP 0x0806
#define ETH_IP 0x0800
#define ARP_REQUEST 1
#define ARP_REPLY 2
#define IP_ICMP 1
#define IP_TCP 6
#define IP_UDP 17

struct arp_hdr {
    uint16 htype, ptype;
    uint8 hlen, plen;
    uint16 oper;
    uint8 sha[6];
    uint32 spa;
    uint8 tha[6];
    uint32 tpa;
} __attribute__((packed));

struct ip_hdr {
    uint8 ver_ihl, tos;
    uint16 total_len, id, frag;
    uint8 ttl, proto;
    uint16 sum;
    uint32 src, dst;
} __attribute__((packed));

struct icmp_hdr {
    uint8 type, code;
    uint16 sum, id, seq;
} __attribute__((packed));

struct udp_hdr {
    uint16 src, dst, len, sum;
} __attribute__((packed));

struct arp_ent {
    net_ipv4_t ip;
    uint8 mac[6];
    uint32 ttl;
    bool used;
};
static struct arp_ent arp[NET_ARP_CACHE_SIZE];
static volatile bool ping_seen;
static uint16 ping_id, ping_seq;

struct udp_msg {
    bool used;
    net_ipv4_t src;
    uint16 sport, dport;
    size_t len;
    uint8 data[768];
};
static struct udp_msg udpq[8];
static uint16 ipid = 1;

void arp_init(void) {
    memset(arp, 0, sizeof(arp));
}

static void arp_put(net_ipv4_t ip, const uint8 mac[6]) {
    for (int i = 0; i < NET_ARP_CACHE_SIZE; i++)
        if (arp[i].used && arp[i].ip == ip) {
            memcpy(arp[i].mac, mac, 6);
            arp[i].ttl = 60000;
            return;
        }
    for (int i = 0; i < NET_ARP_CACHE_SIZE; i++)
        if (!arp[i].used) {
            arp[i].used = true;
            arp[i].ip = ip;
            memcpy(arp[i].mac, mac, 6);
            arp[i].ttl = 60000;
            return;
        }
}

static void arp_request(net_ipv4_t ip) {
    struct arp_hdr h;
    uint8 bc[6] = {255, 255, 255, 255, 255, 255};
    memset(&h, 0, sizeof(h));
    h.htype = net_htons(1);
    h.ptype = net_htons(ETH_IP);
    h.hlen = 6;
    h.plen = 4;
    h.oper = net_htons(ARP_REQUEST);
    memcpy(h.sha, net_cfg.mac, 6);
    h.spa = net_htonl(net_cfg.ip);
    h.tpa = net_htonl(ip);
    ethernet_send(ETH_ARP, bc, &h, sizeof(h));
}

void arp_input(const uint8 *p, size_t len) {
    if (len < sizeof(struct arp_hdr))
        return;
    const struct arp_hdr *h = (const struct arp_hdr *)p;
    if (net_ntohs(h->htype) != 1 || net_ntohs(h->ptype) != ETH_IP || h->hlen != 6 || h->plen != 4)
        return;
    net_ipv4_t spa = net_ntohl(h->spa), tpa = net_ntohl(h->tpa);
    arp_put(spa, h->sha);
    if (net_ntohs(h->oper) == ARP_REQUEST && tpa == net_cfg.ip) {
        struct arp_hdr r = *h;
        r.oper = net_htons(ARP_REPLY);
        memcpy(r.tha, h->sha, 6);
        r.tpa = h->spa;
        memcpy(r.sha, net_cfg.mac, 6);
        r.spa = net_htonl(net_cfg.ip);
        ethernet_send(ETH_ARP, h->sha, &r, sizeof(r));
    }
}

int arp_resolve(net_ipv4_t ip, uint8 mac[6]) {
    for (int i = 0; i < NET_ARP_CACHE_SIZE; i++)
        if (arp[i].used && arp[i].ip == ip) {
            memcpy(mac, arp[i].mac, 6);
            return NET_OK;
        }
    arp_request(ip);
    for (int t = 0; t < 2000; t++) {
        netif_poll();
        for (int i = 0; i < NET_ARP_CACHE_SIZE; i++)
            if (arp[i].used && arp[i].ip == ip) {
                memcpy(mac, arp[i].mac, 6);
                return NET_OK;
            }
    }
    return NET_ETIMEDOUT;
}

void arp_tick(void) {
    for (int i = 0; i < NET_ARP_CACHE_SIZE; i++)
        if (arp[i].used && arp[i].ttl > 0 && --arp[i].ttl == 0)
            arp[i].used = false;
}

void ipv4_init(net_ipv4_t ip, net_ipv4_t mask, net_ipv4_t gw, net_ipv4_t dns) {
    net_cfg.ip = ip;
    net_cfg.netmask = mask;
    net_cfg.gateway = gw;
    net_cfg.dns = dns;
    netif_init();
    arp_init();
}

void ipv4_input(const uint8 *p, size_t len) {
    if (len < sizeof(struct ip_hdr))
        return;
    const struct ip_hdr *h = (const struct ip_hdr *)p;
    size_t ihl = (h->ver_ihl & 15) * 4;
    if ((h->ver_ihl >> 4) != 4 || ihl < 20 || len < ihl)
        return;
    uint16 tot = net_ntohs(h->total_len);
    if (tot > len || tot < ihl)
        return;
    if (net_checksum(p, ihl) != 0)
        return;
    net_ipv4_t src = net_ntohl(h->src);
    const uint8 *pl = p + ihl;
    size_t plen = tot - ihl;
    if (h->proto == IP_ICMP)
        icmp_input(src, pl, plen);
    else if (h->proto == IP_UDP)
        udp_input(src, pl, plen);
    else if (h->proto == IP_TCP)
        tcp_input(src, pl, plen);
}

int ipv4_send(net_ipv4_t dst, uint8 proto, const void *payload, size_t len) {
    uint8 mac[6];
    net_ipv4_t nh = ((dst & net_cfg.netmask) == (net_cfg.ip & net_cfg.netmask)) ? dst : net_cfg.gateway;
    if (arp_resolve(nh, mac) != NET_OK)
        return NET_ETIMEDOUT;
    uint8 buf[20 + NET_MTU];
    if (len > NET_MTU - 20)
        return NET_EINVAL;
    struct ip_hdr *h = (struct ip_hdr *)buf;
    memset(h, 0, 20);
    h->ver_ihl = 0x45;
    h->total_len = net_htons(20 + len);
    h->id = net_htons(ipid++);
    h->ttl = 64;
    h->proto = proto;
    h->src = net_htonl(net_cfg.ip);
    h->dst = net_htonl(dst);
    memcpy(buf + 20, payload, len);
    h->sum = net_htons(net_checksum(h, 20));
    return ethernet_send(ETH_IP, mac, buf, 20 + len);
}

void icmp_input(net_ipv4_t src, const uint8 *p, size_t len) {
    if (len < 4)
        return;
    const struct icmp_hdr *h = (const struct icmp_hdr *)p;
    if (net_checksum(p, len) != 0)
        return;
    if (h->type == 8) {
        uint8 b[1500];
        if (len > sizeof(b))
            return;
        memcpy(b, p, len);
        struct icmp_hdr *r = (struct icmp_hdr *)b;
        r->type = 0;
        r->sum = 0;
        r->sum = net_htons(net_checksum(b, len));
        ipv4_send(src, IP_ICMP, b, len);
    } else if (h->type == 0 && h->id == net_htons(ping_id) && h->seq == net_htons(ping_seq))
        ping_seen = true;
}

int icmp_ping(net_ipv4_t dst, uint16 id, uint16 seq, uint32 timeout) {
    uint8 b[32];
    struct icmp_hdr *h = (struct icmp_hdr *)b;
    memset(b, 0, sizeof(b));
    h->type = 8;
    h->id = net_htons(id);
    h->seq = net_htons(seq);
    h->sum = net_htons(net_checksum(b, sizeof(b)));
    ping_seen = false;
    ping_id = id;
    ping_seq = seq;
    if (ipv4_send(dst, IP_ICMP, b, sizeof(b)) != NET_OK)
        return NET_ERR;
    for (uint32 t = 0; t < timeout; t++) {
        netif_poll();
        if (ping_seen)
            return NET_OK;
    }
    return NET_ETIMEDOUT;
}

void udp_input(net_ipv4_t src, const uint8 *p, size_t len) {
    if (len < sizeof(struct udp_hdr))
        return;
    const struct udp_hdr *h = (const struct udp_hdr *)p;
    size_t ulen = net_ntohs(h->len);
    if (ulen < 8 || ulen > len)
        return;
    for (int i = 0; i < 8; i++)
        if (!udpq[i].used) {
            udpq[i].used = true;
            udpq[i].src = src;
            udpq[i].sport = net_ntohs(h->src);
            udpq[i].dport = net_ntohs(h->dst);
            udpq[i].len = ulen - 8;
            if (udpq[i].len > sizeof(udpq[i].data))
                udpq[i].len = sizeof(udpq[i].data);
            memcpy(udpq[i].data, p + 8, udpq[i].len);
            break;
        }
}

int udp_send(net_ipv4_t dst, uint16 sport, uint16 dport, const void *data, size_t len) {
    uint8 b[1500];
    if (len > 1472)
        return NET_EINVAL;
    struct udp_hdr *h = (struct udp_hdr *)b;
    h->src = net_htons(sport);
    h->dst = net_htons(dport);
    h->len = net_htons(len + 8);
    h->sum = 0;
    memcpy(b + 8, data, len);
    return ipv4_send(dst, IP_UDP, b, len + 8);
}

int udp_recv(uint16 port, net_ipv4_t *src, uint16 *sport, uint8 *buf, size_t *len, uint32 timeout) {
    for (uint32 t = 0; t < timeout; t++) {
        netif_poll();
        for (int i = 0; i < 8; i++)
            if (udpq[i].used && udpq[i].dport == port) {
                size_t n = udpq[i].len;
                if (*len < n)
                    n = *len;
                memcpy(buf, udpq[i].data, n);
                *len = n;
                if (src)
                    *src = udpq[i].src;
                if (sport)
                    *sport = udpq[i].sport;
                udpq[i].used = false;
                return NET_OK;
            }
    }
    return NET_ETIMEDOUT;
}
