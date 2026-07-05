/**
 * @file 8139.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The driver for RTL8139 Networking Card.
 * @version 0.1
 * @date 2023-12-05
 *
 * @copyright Copyright (c) Pradosh 2023
 *
 */
#include <drivers/rtl8139.h>
#include <heap.h>
#include <memory.h>
#include <net/net.h>
#include <paging.h>

#define RTL8139_REG_MAR0 0x08
#define RTL8139_REG_RBSTART 0x30
#define RTL8139_REG_CAPR 0x38
#define RTL8139_REG_CBR 0x3A
#define RTL8139_REG_IMR 0x3C
#define RTL8139_REG_ISR 0x3E
#define RTL8139_REG_TCR 0x40
#define RTL8139_REG_RCR 0x44
#define RTL8139_REG_CONFIG1 0x52

#define RTL8139_ISR_ROK 0x0001
#define RTL8139_ISR_TOK 0x0004
#define RTL8139_ISR_RXOVW 0x0010
#define RTL8139_ISR_TER 0x0008
#define RTL8139_ISR_RER 0x0002

#define RTL8139_RCR_AAP 0x00000001
#define RTL8139_RCR_APM 0x00000002
#define RTL8139_RCR_AM 0x00000004
#define RTL8139_RCR_AB 0x00000008
#define RTL8139_RCR_WRAP 0x00000080
#define RTL8139_RCR_MXDMA_UNLIMITED (7U << 8)
#define RTL8139_RCR_RBLEN_8K (0U << 11)

#define RTL8139_TX_DESC_COUNT 4
#define RTL8139_TX_BUFFER_SIZE 2048
#define RTL8139_RX_BUFFER_SIZE 8192
#define RTL8139_RX_READ_POINTER_GAP 16

struct rtl8139 *RTL8139 = NULL;
static uint8 *rx_buffer;
static uint8 *tx_buffers[RTL8139_TX_DESC_COUNT];
static uint8 tx_cur;
static uint16 rx_cur;
static bool rtl8139_ready;

void read_mac_address() {
    for (int i = 0; i < 6; i++) {
        RTL8139->mac_address[i] = inb(RTL8139->io_base + RTL8139_REG_MAC + i);
    }
}

// Initialize RTL8139 NIC
void rtl8139_init(struct rtl8139 *nic) {
    if (!nic || nic->io_base == null || nic->io_base == 0) {
        warn("RTL8139 Card is not detected but tried to initialize it. Skipping...", __FILE__);
        return;
    }
    rtl8139_ready = false;
    info("Initialization started!", __FILE__);

    outb(nic->io_base + RTL8139_REG_CONFIG1, 0x00);

    // Reset the NIC and wait until the reset bit is cleared.
    outb(nic->io_base + RTL8139_REG_COMMAND, RTL8139_CMD_RESET);
    for (uint32 t = 0; t < 100000 && (inb(nic->io_base + RTL8139_REG_COMMAND) & RTL8139_CMD_RESET); t++)
        ;

    read_mac_address();
    printf("Mac Address : %x:%x:%x:%x:%x:%x", nic->mac_address[0], nic->mac_address[1], nic->mac_address[2], nic->mac_address[3], nic->mac_address[4], nic->mac_address[5]);

    rx_buffer = kmalloc_aligned(RTL8139_RX_BUFFER_SIZE + 16 + 1500, 256);
    if (!rx_buffer) {
        warn("RTL8139 failed to allocate receive buffer", __FILE__);
        return;
    }
    memset(rx_buffer, 0, RTL8139_RX_BUFFER_SIZE + 16 + 1500);
    outl(nic->io_base + RTL8139_REG_RBSTART, (uint32)fast_virt_to_phys(rx_buffer));

    for (int i = 0; i < RTL8139_TX_DESC_COUNT; i++) {
        /* kmalloc_aligned rejects alignments smaller than sizeof(void *). */
        tx_buffers[i] = kmalloc_aligned(RTL8139_TX_BUFFER_SIZE, 256);
        if (!tx_buffers[i]) {
            warn("RTL8139 failed to allocate transmit buffer", __FILE__);
            return;
        }
        outl(nic->io_base + RTL8139_REG_TX_ADDR + (i * 4), (uint32)fast_virt_to_phys(tx_buffers[i]));
    }
    tx_cur = 0;
    rx_cur = 0;

    // Accept packets for this MAC, broadcasts, and multicast ARP/DNS traffic.
    outl(nic->io_base + RTL8139_REG_RCR,
        RTL8139_RCR_APM | RTL8139_RCR_AM | RTL8139_RCR_AB |
            RTL8139_RCR_WRAP | RTL8139_RCR_MXDMA_UNLIMITED | RTL8139_RCR_RBLEN_8K);
    outl(nic->io_base + RTL8139_REG_TCR, 0x00000700);
    outw(nic->io_base + RTL8139_REG_IMR, RTL8139_ISR_ROK | RTL8139_ISR_TOK | RTL8139_ISR_RXOVW | RTL8139_ISR_TER | RTL8139_ISR_RER);
    outw(nic->io_base + RTL8139_REG_ISR, 0xFFFF);

    // Enable receive and transmit.
    outb(nic->io_base + RTL8139_REG_COMMAND, RTL8139_CMD_RX_ENABLE | RTL8139_CMD_TX_ENABLE);
    rtl8139_ready = true;
    done("Successfully Initialized!", __FILE__);
}

// Transmit a packet
bool rtl8139_send_packet(const uint8 *data, uint16 length) {
    if (!rtl8139_ready || !RTL8139 || RTL8139->io_base == null || RTL8139->io_base == 0 || !data || length == 0) {
        warn("RTL8139 Card is not ready but tried to send data. Skipping...", __FILE__);
        return no;
    }
    if (length > RTL8139_TX_BUFFER_SIZE)
        return no;

    uint8 desc = tx_cur;
    if (!tx_buffers[desc])
        return no;

    uint16 status_port = RTL8139->io_base + RTL8139_REG_TX_STATUS + (desc * 4);
    uint32 status = inl(status_port);
    if (status != 0 && (status & (1U << 13)) == 0 && (status & (1U << 15)) == 0)
        return no;

    memcpy(tx_buffers[desc], data, length);
    outl(status_port, length);
    tx_cur = (tx_cur + 1) % RTL8139_TX_DESC_COUNT;
    return yes;
}

// Receives a packet
bool rtl8139_receive_packet(uint8 *buffer, uint16 *length) {
    if (!rtl8139_ready || !RTL8139 || RTL8139->io_base == null || RTL8139->io_base == 0 || !rx_buffer || !buffer || !length) {
        return no;
    }
    if (inb(RTL8139->io_base + RTL8139_REG_COMMAND) & 0x01)
        return no;

    uint16 packet_status = *(volatile uint16 *)(rx_buffer + rx_cur);
    uint16 packet_len = *(volatile uint16 *)(rx_buffer + rx_cur + 2);
    if ((packet_status & RTL8139_ISR_ROK) == 0 || packet_len < 4 || packet_len > NET_FRAME_MAX + 4) {
        outw(RTL8139->io_base + RTL8139_REG_ISR, RTL8139_ISR_ROK | RTL8139_ISR_RER | RTL8139_ISR_RXOVW);
        return no;
    }

    uint16 frame_len = packet_len - 4;
    uint16 frame_off = (rx_cur + 4) % RTL8139_RX_BUFFER_SIZE;
    for (uint16 i = 0; i < frame_len; i++)
        buffer[i] = rx_buffer[(frame_off + i) % RTL8139_RX_BUFFER_SIZE];
    *length = frame_len;

    rx_cur = (rx_cur + packet_len + 4 + 3) & ~3U;
    rx_cur %= RTL8139_RX_BUFFER_SIZE;
    outw(RTL8139->io_base + RTL8139_REG_CAPR, (rx_cur - RTL8139_RX_READ_POINTER_GAP) % RTL8139_RX_BUFFER_SIZE);
    outw(RTL8139->io_base + RTL8139_REG_ISR, RTL8139_ISR_ROK | RTL8139_ISR_RER | RTL8139_ISR_RXOVW);
    return yes;
}
