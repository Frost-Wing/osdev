/**
 * @file 8139.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The driver for RTL8139 Networking Card.
 * @version 0.1
 * @date 2023-12-05
 *
 * @copyright Copyright (c) Pradosh 2023-2026
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
#define RTL8139_RCR_RBLEN_64K (3U << 11)
#define RTL8139_RX_WRAP_PAD 2048

#define RTL8139_TX_DESC_COUNT 4
#define RTL8139_TX_BUFFER_SIZE 2048
#define RTL8139_RX_BUFFER_SIZE 65536
#define RTL8139_RX_READ_POINTER_GAP 16
#define RTL8139_RXQ_LEN 64

struct rtl8139_rxq_entry {
    uint16 len;
    uint8 data[NET_FRAME_MAX];
};

struct rtl8139 *RTL8139 = NULL;
static uint8 *rx_buffer;
static uint8 *tx_buffers[RTL8139_TX_DESC_COUNT];
static uint8 tx_cur;
static uint16 rx_cur;
static uint32 rtl8139_rcr_value;
static struct rtl8139_rxq_entry rtl8139_rxq[RTL8139_RXQ_LEN];
static volatile uint32 rtl8139_rxq_head;
static volatile uint32 rtl8139_rxq_tail;
static volatile bool rtl8139_rx_reset_pending;
static bool rtl8139_ready;
static bool rtl8139_irq_driven = false;

void read_mac_address() {
    for (int i = 0; i < 6; i++) {
        RTL8139->mac_address[i] = inb(RTL8139->io_base + RTL8139_REG_MAC + i);
    }
}

static void rtl8139_rx_reset(void) {
    uint16 io = RTL8139->io_base;
    outb(io + RTL8139_REG_COMMAND, RTL8139_CMD_TX_ENABLE);
    rx_cur = 0;
    memset(rx_buffer, 0, RTL8139_RX_BUFFER_SIZE + 16 + RTL8139_RX_WRAP_PAD);
    outl(io + RTL8139_REG_RBSTART, (uint32)fast_virt_to_phys(rx_buffer));
    outl(io + RTL8139_REG_RCR, rtl8139_rcr_value);
    outw(io + RTL8139_REG_CAPR, (uint16)(0 - RTL8139_RX_READ_POINTER_GAP));
    outw(io + RTL8139_REG_ISR, 0xFFFF);
    outb(io + RTL8139_REG_COMMAND, RTL8139_CMD_RX_ENABLE | RTL8139_CMD_TX_ENABLE);
    netif_stats.rx_resets++;
}

void rtl8139_rx_to_queue(void) {
    while ((uint32)(rtl8139_rxq_head - rtl8139_rxq_tail) < RTL8139_RXQ_LEN) {
        struct rtl8139_rxq_entry *entry =
            &rtl8139_rxq[rtl8139_rxq_head % RTL8139_RXQ_LEN];
        uint16 len = 0;
        if (!rtl8139_receive_packet(entry->data, &len))
            break;
        if (len < 14 || len > NET_FRAME_MAX) {
            netif_stats.rx_queue_drops++;
            continue;
        }
        entry->len = len;
        __sync_synchronize();
        rtl8139_rxq_head++;
    }
}

bool rtl8139_receive_queued_packet(uint8 *buffer, uint16 *length) {
    if (!buffer || !length || rtl8139_rxq_tail == rtl8139_rxq_head)
        return no;
    struct rtl8139_rxq_entry *entry =
        &rtl8139_rxq[rtl8139_rxq_tail % RTL8139_RXQ_LEN];
    uint16 len = entry->len;
    memcpy(buffer, entry->data, len);
    *length = len;
    __sync_synchronize();
    rtl8139_rxq_tail++;
    return yes;
}

void rtl8139_service_pending_rx_reset(void) {
    if (rtl8139_rx_reset_pending) {
        rtl8139_rx_reset_pending = false;
        rtl8139_rx_reset();
    }
}

// Initialize RTL8139 NIC
void rtl8139_init(struct rtl8139 *nic) {
    LOG_SCOPE();
    if (!nic || nic->io_base == null || nic->io_base == 0) {
        warn("RTL8139 Card is not detected but tried to initialize it. Skipping...", __FILE__);
        return;
    }
    rtl8139_ready = false;
    rtl8139_irq_driven = false;
    info("Initialization started!", __FILE__);

    outb(nic->io_base + RTL8139_REG_CONFIG1, 0x00);

    // Reset the NIC and wait until the reset bit is cleared.
    outb(nic->io_base + RTL8139_REG_COMMAND, RTL8139_CMD_RESET);
    for (uint32 t = 0; t < 100000 && (inb(nic->io_base + RTL8139_REG_COMMAND) & RTL8139_CMD_RESET); t++)
        ;

    read_mac_address();
    info("Mac Address : %x:%x:%x:%x:%x:%x", __FILE__ , nic->mac_address[0], nic->mac_address[1], nic->mac_address[2], nic->mac_address[3], nic->mac_address[4], nic->mac_address[5]);

    rx_buffer = kmalloc_aligned(RTL8139_RX_BUFFER_SIZE + 16 + RTL8139_RX_WRAP_PAD, 256);
    if (!rx_buffer) {
        warn("RTL8139 failed to allocate receive buffer", __FILE__);
        return;
    }
    uint64 rx_phys = fast_virt_to_phys(rx_buffer);
    if (rx_phys > UINT32_MAX ||
        RTL8139_RX_BUFFER_SIZE + 16 + RTL8139_RX_WRAP_PAD > UINT32_MAX - rx_phys) {
        warn("RTL8139 receive buffer must be physically addressable below 4 GiB", __FILE__);
        return;
    }
    memset(rx_buffer, 0, RTL8139_RX_BUFFER_SIZE + 16 + RTL8139_RX_WRAP_PAD);
    outl(nic->io_base + RTL8139_REG_RBSTART, (uint32)rx_phys);

    for (int i = 0; i < RTL8139_TX_DESC_COUNT; i++) {
        /* kmalloc_aligned rejects alignments smaller than sizeof(void *). */
        tx_buffers[i] = kmalloc_aligned(RTL8139_TX_BUFFER_SIZE, 256);
        if (!tx_buffers[i]) {
            warn("RTL8139 failed to allocate transmit buffer", __FILE__);
            return;
        }
        uint64 tx_phys = fast_virt_to_phys(tx_buffers[i]);
        if (tx_phys > UINT32_MAX ||
            RTL8139_TX_BUFFER_SIZE > UINT32_MAX - tx_phys) {
            warn("RTL8139 transmit buffer must be physically addressable below 4 GiB", __FILE__);
            return;
        }
        outl(nic->io_base + RTL8139_REG_TX_ADDR + (i * 4), (uint32)tx_phys);
    }
    tx_cur = 0;
    rx_cur = 0;
    rtl8139_rxq_head = 0;
    rtl8139_rxq_tail = 0;
    rtl8139_rx_reset_pending = false;

    // Accept packets for this MAC, broadcasts, and multicast ARP/DNS traffic.
    rtl8139_rcr_value = RTL8139_RCR_APM | RTL8139_RCR_AM | RTL8139_RCR_AB |
        RTL8139_RCR_WRAP | RTL8139_RCR_MXDMA_UNLIMITED | RTL8139_RCR_RBLEN_64K;
    outl(nic->io_base + RTL8139_REG_RCR, rtl8139_rcr_value);
    outl(nic->io_base + RTL8139_REG_TCR, 0x00000700);
    
    // Enable interrupts for RX ready, TX OK, RX overflow, TX error, RX error
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

// Receives a packet (polling mode)
bool rtl8139_receive_packet(uint8 *buffer, uint16 *length) {
    if (!rtl8139_ready || !RTL8139 || RTL8139->io_base == null || RTL8139->io_base == 0 || !rx_buffer || !buffer || !length) {
        return no;
    }
    if (inb(RTL8139->io_base + RTL8139_REG_COMMAND) & 0x01)
        return no;

    uint16 packet_status = *(volatile uint16 *)(rx_buffer + rx_cur);
    uint16 packet_len = *(volatile uint16 *)(rx_buffer + rx_cur + 2);
    if ((packet_status & RTL8139_ISR_ROK) == 0 || packet_len < 4 || packet_len > NET_FRAME_MAX + 4) {
        netif_stats.rx_bad_header++;
        rtl8139_rx_reset_pending = true;
        return no;
    }

    uint16 frame_len = packet_len - 4;
    memcpy(buffer, rx_buffer + rx_cur + 4, frame_len);
    *length = frame_len;

    rx_cur = (rx_cur + packet_len + 4 + 3) & ~3U;
    rx_cur %= RTL8139_RX_BUFFER_SIZE;
    outw(RTL8139->io_base + RTL8139_REG_CAPR, (uint16)(rx_cur - RTL8139_RX_READ_POINTER_GAP));
    outw(RTL8139->io_base + RTL8139_REG_ISR, RTL8139_ISR_ROK | RTL8139_ISR_RER | RTL8139_ISR_RXOVW);
    return yes;
}

/**
 * @brief Interrupt handler for RTL8139 NIC
 * Called when hardware raises IRQ with ROK (receive ok) or other status bits
 * Queues received packets for processing outside interrupt context
 */
void rtl8139_interrupt_handler(void) {
    if (!rtl8139_ready || !RTL8139 || RTL8139->io_base == null)
        return;

    uint16_t status = inw(RTL8139->io_base + RTL8139_REG_ISR);
    outw(RTL8139->io_base + RTL8139_REG_ISR, status);

    if (status & (RTL8139_ISR_ROK | RTL8139_ISR_RXOVW))
        rtl8139_rx_to_queue();
    if (status & RTL8139_ISR_RXOVW) {
        netif_stats.rx_overflow++;
        rtl8139_rx_reset_pending = true;
    }

    if (status & RTL8139_ISR_TOK) {
    }

    if (status & (RTL8139_ISR_TER | RTL8139_ISR_RER)) {
        netif_stats.rx_errors++;
    }
}

void rtl8139_irq_enable(void) {
    if (!RTL8139 || RTL8139->io_base == null)
        return;
    rtl8139_irq_driven = true;
    // IMR already set in rtl8139_init, just mark as active
}

void rtl8139_irq_disable(void) {
    rtl8139_irq_driven = false;
}

bool rtl8139_is_irq_driven(void) {
    return rtl8139_irq_driven;
}
