#include <drivers/rtl8139.h>
#include <graphics.h>
#include <memory.h>
#include <net/net.h>
#include <strings.h>

struct net_config net_cfg;
net_rx_callback_t rx_cb;

uint16 net_htons(uint16 v) {
    return (uint16)((v << 8) | (v >> 8));
}

uint16 net_ntohs(uint16 v) {
    return net_htons(v);
}

uint32 net_htonl(uint32 v) {
    return ((v & 0xff) << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) | ((v >> 24) & 0xff);
}

uint32 net_ntohl(uint32 v) {
    return net_htonl(v);
}

net_ipv4_t net_ipv4_from_octets(uint8 a, uint8 b, uint8 c, uint8 d) {
    return ((uint32)a << 24) | ((uint32)b << 16) | ((uint32)c << 8) | d;
}

int net_parse_ipv4(const char *s, net_ipv4_t *out) {
    uint32 parts[4] = {0};
    int part = 0;
    uint32 val = 0;
    bool have = false;
    if (!s || !out)
        return NET_EINVAL;
    for (size_t i = 0;; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            val = val * 10 + (uint32)(c - '0');
            if (val > 255)
                return NET_EINVAL;
            have = true;
        } else if (c == '.' || c == '\0') {
            if (!have || part >= 4)
                return NET_EINVAL;
            parts[part++] = val;
            val = 0;
            have = false;
            if (c == '\0')
                break;
        } else
            return NET_EINVAL;
    }
    if (part != 4)
        return NET_EINVAL;
    *out = net_ipv4_from_octets(parts[0], parts[1], parts[2], parts[3]);
    return NET_OK;
}

void net_format_ipv4(net_ipv4_t ip, char *out, size_t out_len) {
    if (!out || out_len == 0)
        return;
    snprintf(out, out_len, "%d.%d.%d.%d", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}

uint16 net_checksum(const void *data, size_t len) {
    const uint8 *p = (const uint8 *)data;
    uint32 sum = 0;
    while (len > 1) {
        sum += ((uint16)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }
    if (len)
        sum += ((uint16)p[0] << 8);
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (uint16)~sum;
}

void net_debug(const char *layer, const char *msg) {
    if (NET_DEBUG) {
        printf("[net:%s] %s", layer, msg);
    }
}

void net_packet_init(net_packet_t *pkt) {
    memset(pkt, 0, sizeof(*pkt));
    pkt->data = pkt->storage + NET_PKT_HEADROOM;
    pkt->capacity = NET_PKT_STORAGE - NET_PKT_HEADROOM;
}

int net_packet_prepend(net_packet_t *pkt, size_t len, void **hdr) {
    if (!pkt || pkt->data < pkt->storage + len)
        return NET_EINVAL;
    pkt->data -= len;
    pkt->len += len;
    if (hdr)
        *hdr = pkt->data;
    return NET_OK;
}

int net_packet_append(net_packet_t *pkt, const void *data, size_t len) {
    if (!pkt || pkt->len + len > pkt->capacity)
        return NET_EINVAL;
    if (data)
        memcpy(pkt->data + pkt->len, data, len);
    else
        memset(pkt->data + pkt->len, 0, len);
    pkt->len += len;
    return NET_OK;
}

int net_packet_pull(net_packet_t *pkt, size_t len, void **hdr) {
    if (!pkt || len > pkt->len)
        return NET_EINVAL;
    if (hdr)
        *hdr = pkt->data;
    pkt->data += len;
    pkt->len -= len;
    return NET_OK;
}

void netif_init(void) {
    netif_get_mac(net_cfg.mac);
    netif_set_rx_callback(ethernet_input);
    // Enable interrupt-driven mode for RTL8139 if available
    rtl8139_irq_enable();
}

int netif_send(const void *frame, size_t len) {
    if (!frame || len > NET_FRAME_MAX)
        return NET_EINVAL;
    return rtl8139_send_packet((const uint8 *)frame, (uint16)len) ? NET_OK : NET_ERR;
}

/**
 * @brief Poll for network packets
 * 
 * If RTL8139 is in interrupt-driven mode, polling is largely a no-op since packets
 * are processed in the IRQ handler. However, we keep this for:
 * 1. Compatibility with existing code
 * 2. Fallback if interrupts are temporarily disabled
 * 3. Safety margin to catch any missed packets
 */
void netif_poll(void) {
    uint8 buf[NET_FRAME_MAX];
    uint16 len = 0;
    // Keep polling loop for backward compatibility and as safety fallback
    // In interrupt-driven mode, most packets are handled by rtl8139_interrupt_handler
    while (rtl8139_receive_packet(buf, &len)) {
        if (rx_cb && len >= 14 && len <= NET_FRAME_MAX)
            rx_cb(buf, len);
        len = 0;
    }
}

void netif_set_rx_callback(net_rx_callback_t cb) {
    rx_cb = cb;
}

void netif_get_mac(uint8 mac[6]) {
    if (!mac)
        return;
    if (RTL8139)
        memcpy(mac, RTL8139->mac_address, 6);
    else
        memset(mac, 0, 6);
}
