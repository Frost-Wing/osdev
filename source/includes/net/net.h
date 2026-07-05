#ifndef NET_NET_H
#define NET_NET_H

#include <basics.h>
#include <stdbool.h>
#include <stddef.h>

#define NET_MTU 1500
#define NET_FRAME_MAX 1518
#define NET_PKT_HEADROOM 64
#define NET_PKT_STORAGE (NET_PKT_HEADROOM + NET_FRAME_MAX)
#define NET_ARP_CACHE_SIZE 16
#define NET_TCP_MAX_SOCKETS 8
#define NET_DEBUG 1

#define NET_OK 0
#define NET_ERR (-1)
#define NET_EINVAL (-2)
#define NET_ENOMEM (-3)
#define NET_ETIMEDOUT (-4)
#define NET_ENOTSUP (-5)

typedef uint32 net_ipv4_t;

typedef struct net_packet {
    uint8 storage[NET_PKT_STORAGE];
    uint8 *data;
    size_t len;
    size_t capacity;
} net_packet_t;

typedef void (*net_rx_callback_t)(const uint8 *frame, size_t len);

struct net_config {
    uint8 mac[6];
    net_ipv4_t ip;
    net_ipv4_t netmask;
    net_ipv4_t gateway;
    net_ipv4_t dns;
};

extern struct net_config net_cfg;

uint16 net_htons(uint16 v);
uint16 net_ntohs(uint16 v);
uint32 net_htonl(uint32 v);
uint32 net_ntohl(uint32 v);
net_ipv4_t net_ipv4_from_octets(uint8 a, uint8 b, uint8 c, uint8 d);
int net_parse_ipv4(const char *s, net_ipv4_t *out);
void net_format_ipv4(net_ipv4_t ip, char *out, size_t out_len);
uint16 net_checksum(const void *data, size_t len);
void net_debug(const char *layer, const char *msg);

void net_packet_init(net_packet_t *pkt);
int net_packet_prepend(net_packet_t *pkt, size_t len, void **hdr);
int net_packet_append(net_packet_t *pkt, const void *data, size_t len);
int net_packet_pull(net_packet_t *pkt, size_t len, void **hdr);

void netif_init(void);
int netif_send(const void *frame, size_t len);
void netif_poll(void);
void netif_set_rx_callback(net_rx_callback_t cb);
void netif_get_mac(uint8 mac[6]);

void ethernet_input(const uint8 *frame, size_t len);
int ethernet_send(uint16 ethertype, const uint8 dst[6], const void *payload, size_t len);

void arp_init(void);
void arp_input(const uint8 *payload, size_t len);
int arp_resolve(net_ipv4_t ip, uint8 mac[6]);
void arp_tick(void);

void ipv4_init(net_ipv4_t ip, net_ipv4_t mask, net_ipv4_t gw, net_ipv4_t dns);
void ipv4_input(const uint8 *payload, size_t len);
int ipv4_send(net_ipv4_t dst, uint8 proto, const void *payload, size_t len);

void icmp_input(net_ipv4_t src, const uint8 *payload, size_t len);
int icmp_ping(net_ipv4_t dst, uint16 id, uint16 seq, uint32 timeout_ticks);

void udp_input(net_ipv4_t src, const uint8 *payload, size_t len);
int udp_send(net_ipv4_t dst, uint16 src_port, uint16 dst_port, const void *data, size_t len);
int udp_recv(uint16 port, net_ipv4_t *src, uint16 *src_port, uint8 *buf, size_t *len, uint32 timeout_ticks);

int dns_resolve(const char *host, net_ipv4_t *out_ip);
int tcp_connect(net_ipv4_t dst, uint16 dst_port);
int tcp_send(int sock, const void *data, size_t len);
int tcp_recv(int sock, uint8 *buf, size_t *len, uint32 timeout_ticks);
void tcp_close(int sock);
void tcp_input(net_ipv4_t src, const uint8 *payload, size_t len);

int http_get_to_file(const char *url, const char *path);
int dhcp_configure(uint32 timeout_ticks);

#endif
