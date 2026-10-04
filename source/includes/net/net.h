/**
 * @file net.h
 * @brief Network packet, protocol, and socket interfaces.
 */
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
#define NET_EINTR (-6)
#define NET_ECONNRESET (-7)
#define NET_EPIPE (-8)
#define NET_EOF 1000

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

typedef struct {
    uint64 rx_bytes;
    uint64 rx_packets;
    uint64 rx_errors;
    uint64 tx_bytes;
    uint64 tx_packets;
    uint64 tx_errors;
    uint64 rx_resets;
    uint64 rx_overflow;
    uint64 rx_bad_header;
    uint64 rx_queue_drops;
    uint64 ip_bad_checksum;
    uint64 udp_bad_checksum;
    uint64 tcp_bad_checksum;
    uint64 tcp_rst_rx;
    uint64 tcp_rst_ignored;
    uint64 tcp_ooo_drops;
    uint64 tcp_rx_full_drops;
    uint64 tcp_retransmits;
    uint64 udp_no_socket_drops;
} netif_stats_t;

extern struct net_config net_cfg;
extern netif_stats_t netif_stats;

typedef void (*wget_progress_cb)(uint64 downloaded, uint64 total, void *ctx);

/**
 * @brief Convert a 16-bit integer from host to network byte order.
 * @param v Host-order value.
 * @return Value in network byte order.
 */
uint16 net_htons(uint16 v);

/**
 * @brief Convert a 16-bit integer from network to host byte order.
 * @param v Network-order value.
 * @return Value in host byte order.
 */
uint16 net_ntohs(uint16 v);

/**
 * @brief Convert a 32-bit integer from host to network byte order.
 * @param v Host-order value.
 * @return Value in network byte order.
 */
uint32 net_htonl(uint32 v);

/**
 * @brief Convert a 32-bit integer from network to host byte order.
 * @param v Network-order value.
 * @return Value in host byte order.
 */
uint32 net_ntohl(uint32 v);

/**
 * @brief Construct an IPv4 address from its four octets.
 * @param a First octet.
 * @param b Second octet.
 * @param c Third octet.
 * @param d Fourth octet.
 * @return The packed IPv4 address.
 */
net_ipv4_t net_ipv4_from_octets(uint8 a, uint8 b, uint8 c, uint8 d);

/**
 * @brief Parse a dotted-decimal IPv4 address.
 * @param s Address string to parse.
 * @param out Receives the parsed address.
 * @return 0 on success, otherwise a network error code.
 */
int net_parse_ipv4(const char *s, net_ipv4_t *out);

/**
 * @brief Format an IPv4 address as dotted-decimal text.
 * @param ip Address to format.
 * @param out Buffer to receive the null-terminated result.
 * @param out_len Size of @p out.
 */
void net_format_ipv4(net_ipv4_t ip, char *out, size_t out_len);

/**
 * @brief Compute the Internet checksum of a data block.
 * @param data Data to checksum.
 * @param len Number of bytes in @p data.
 * @return The 16-bit checksum.
 */
uint16 net_checksum(const void *data, size_t len);

/**
 * @brief Emit a network debugging message.
 * @param layer Protocol or subsystem name.
 * @param msg Message to emit.
 */
void net_debug(const char *layer, const char *msg);

/**
 * @brief Initialize an empty packet buffer.
 * @param pkt Packet to initialize.
 */
void net_packet_init(net_packet_t *pkt);

/**
 * @brief Reserve space at the start of a packet.
 * @param pkt Packet to modify.
 * @param len Number of bytes to reserve.
 * @param hdr Receives a pointer to the reserved header space.
 * @return 0 on success, otherwise a network error code.
 */
int net_packet_prepend(net_packet_t *pkt, size_t len, void **hdr);

/**
 * @brief Append bytes to the end of a packet.
 * @param pkt Packet to modify.
 * @param data Bytes to append.
 * @param len Number of bytes to append.
 * @return 0 on success, otherwise a network error code.
 */
int net_packet_append(net_packet_t *pkt, const void *data, size_t len);

/**
 * @brief Remove bytes from the start of a packet.
 * @param pkt Packet to modify.
 * @param len Number of bytes to remove.
 * @param hdr Receives a pointer to the removed data.
 * @return 0 on success, otherwise a network error code.
 */
int net_packet_pull(net_packet_t *pkt, size_t len, void **hdr);

/** @brief Initialize the network interface and protocol state. */
void netif_init(void);

/**
 * @brief Transmit a complete Ethernet frame.
 * @param frame Frame data.
 * @param len Frame length in bytes.
 * @return 0 on success, otherwise a network error code.
 */
int netif_send(const void *frame, size_t len);

/** @brief Account for a received frame of the specified size. */
void netif_account_rx(size_t len);

/** @brief Poll the network interface for received frames and protocol work. */
void netif_poll(void);

/** @brief Set the callback invoked for received frames. */
void netif_set_rx_callback(net_rx_callback_t cb);

/** @brief Copy the interface MAC address into the supplied six-byte buffer. */
void netif_get_mac(uint8 mac[6]);

/** @brief Process a received Ethernet frame. */
void ethernet_input(const uint8 *frame, size_t len);

/**
 * @brief Send an Ethernet payload to a destination MAC address.
 * @param ethertype Ethernet protocol type.
 * @param dst Destination MAC address.
 * @param payload Payload bytes.
 * @param len Payload length in bytes.
 * @return 0 on success, otherwise a network error code.
 */
int ethernet_send(uint16 ethertype, const uint8 dst[6], const void *payload, size_t len);

/** @brief Initialize the ARP cache and protocol state. */
void arp_init(void);

/** @brief Process a received ARP packet. */
void arp_input(const uint8 *payload, size_t len);

/**
 * @brief Resolve an IPv4 address to a MAC address.
 * @param ip IPv4 address to resolve.
 * @param mac Receives the resolved six-byte address.
 * @return 0 if resolved, otherwise a network error code.
 */
int arp_resolve(net_ipv4_t ip, uint8 mac[6]);

/** @brief Advance ARP cache timers. */
void arp_tick(void);

/**
 * @brief Configure the local IPv4 address and network parameters.
 * @param ip Local address.
 * @param mask Network mask.
 * @param gw Default gateway.
 * @param dns DNS server.
 */
void ipv4_init(net_ipv4_t ip, net_ipv4_t mask, net_ipv4_t gw, net_ipv4_t dns);

/** @brief Process a received IPv4 payload. */
void ipv4_input(const uint8 *payload, size_t len);

/**
 * @brief Send an IPv4 packet.
 * @param dst Destination IPv4 address.
 * @param proto IP protocol number.
 * @param payload Payload bytes.
 * @param len Payload length in bytes.
 * @return 0 on success, otherwise a network error code.
 */
int ipv4_send(net_ipv4_t dst, uint8 proto, const void *payload, size_t len);

/** @brief Process a received ICMP packet. */
void icmp_input(net_ipv4_t src, const uint8 *payload, size_t len);

/**
 * @brief Send an ICMP echo request and wait for its response.
 * @param dst Destination IPv4 address.
 * @param id Echo identifier.
 * @param seq Echo sequence number.
 * @param timeout_ticks Maximum wait in timer ticks.
 * @return 0 on success, otherwise a network error code.
 */
int icmp_ping(net_ipv4_t dst, uint16 id, uint16 seq, uint32 timeout_ticks);

/** @brief Process a received UDP datagram. */
void udp_input(net_ipv4_t src, const uint8 *payload, size_t len);

/**
 * @brief Send a UDP datagram.
 * @param dst Destination IPv4 address.
 * @param src_port Source port.
 * @param dst_port Destination port.
 * @param data Datagram payload.
 * @param len Payload length in bytes.
 * @return 0 on success, otherwise a network error code.
 */
int udp_send(net_ipv4_t dst, uint16 src_port, uint16 dst_port, const void *data, size_t len);

/**
 * @brief Receive a queued UDP datagram for a local port.
 * @param port Local destination port.
 * @param src Receives the sender IPv4 address, if non-NULL.
 * @param src_port Receives the sender port, if non-NULL.
 * @param buf Buffer to receive the datagram.
 * @param len Input capacity and output datagram length.
 * @param timeout_ticks Maximum wait in timer ticks.
 * @return 0 on success, otherwise a network error code.
 */
int udp_recv(uint16 port, net_ipv4_t *src, uint16 *src_port, uint8 *buf, size_t *len, uint32 timeout_ticks);

/** @brief Check whether a UDP datagram is queued for a local port. */
bool udp_has_data(uint16 port);

/** @brief Discard queued UDP datagrams for a local port. */
void udp_purge_port(uint16 port);

/**
 * @brief Resolve a host name to an IPv4 address.
 * @param host Host name to resolve.
 * @param out_ip Receives the resolved address.
 * @return 0 on success, otherwise a network error code.
 */
int dns_resolve(const char *host, net_ipv4_t *out_ip);

/**
 * @brief Establish a TCP connection to a remote endpoint.
 * @param dst Remote IPv4 address.
 * @param dst_port Remote TCP port.
 * @return Socket handle on success, otherwise a negative network error code.
 */
int tcp_connect(net_ipv4_t dst, uint16 dst_port);

/**
 * @brief Send data on a TCP connection.
 * @param sock TCP socket handle.
 * @param data Data to send.
 * @param len Number of bytes to send.
 * @return Number of bytes sent, or a negative network error code.
 */
int tcp_send(int sock, const void *data, size_t len);

/**
 * @brief Receive data from a TCP connection.
 * @param sock TCP socket handle.
 * @param buf Buffer to receive data.
 * @param len Input capacity and output number of bytes received.
 * @param timeout_ticks Maximum wait in timer ticks.
 * @return 0 on success, otherwise a network error code.
 */
int tcp_recv(int sock, uint8 *buf, size_t *len, uint32 timeout_ticks);

/** @brief Check whether a TCP socket has received data. */
bool tcp_has_data(int sock);

/** @brief Check whether a TCP connection has closed. */
bool tcp_is_closed(int sock);

/** @brief Check whether a TCP connection is established. */
bool tcp_is_connected(int sock);

/** @brief Close a TCP connection. */
void tcp_close(int sock);

/** @brief Process a received TCP segment. */
void tcp_input(net_ipv4_t src, const uint8 *payload, size_t len);

/**
 * @brief Fetch an HTTP resource and write it to a file.
 * @param url HTTP URL to fetch.
 * @param path Destination file path.
 * @param cb Optional progress callback.
 * @param ctx Opaque value passed to @p cb.
 * @return 0 on success, otherwise a network error code.
 */
int http_get_to_file(const char *url, const char *path,
    wget_progress_cb cb, void *ctx);

/**
 * @brief Obtain network configuration using DHCP.
 * @param timeout_ticks Maximum time to wait in timer ticks.
 * @return 0 on success, otherwise a network error code.
 */
int dhcp_configure(uint32 timeout_ticks);

#endif
