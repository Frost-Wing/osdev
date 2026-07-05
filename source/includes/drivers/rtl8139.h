/**
 * @file rtl8139.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Header files for RTL8139 Networking Card.
 * @version 0.1
 * @date 2023-12-05
 *
 * @copyright Copyright (c) Pradosh 2023
 *
 */
#ifndef rtl8139_h
#define rtl8139_h

#include <basics.h>
#include <graphics.h>
#include <hal.h>

// RTL8139 registers
#define RTL8139_REG_MAC 0x00
#define RTL8139_REG_TX_STATUS 0x10
#define RTL8139_REG_TX_ADDR 0x20
#define RTL8139_REG_RX_BUFFER 0x30
#define RTL8139_REG_COMMAND 0x37

// RTL8139 commands
#define RTL8139_CMD_RESET 0x10
#define RTL8139_CMD_RX_ENABLE 0x09
#define RTL8139_CMD_TX_ENABLE 0x04

// RTL8139 packet buffer size
#define RTL8139_BUFFER_SIZE 8192

// RTL8139 PCI addresses
#define RTL8139_IOADDR1 0x10
#define RTL8139_CMD 0x37

// RTL8139 Status
#define TOK 0x0001
#define ROK 0x0002

#define RTL8139_IRQ_LINE 0x3C

struct rtl8139 {
    uint16 io_base;
    uint8 mac_address[6];
};

/**
 * @brief Global pointer for the RTL card.
 *
 */
extern struct rtl8139 *RTL8139;

/**
 * @brief Function to read the MAC address from EEPROM
 *
 * @param nic
 */
void read_mac_address(void);

/**
 * @brief Initialize RTL8139 NIC with interrupt support
 *
 * @param nic the pointer to RTL structure
 */
void rtl8139_init(struct rtl8139 *nic);

/**
 * @brief Transmit a packet from the RTL8139
 *
 * @param data
 * @param length
 * @return true if successfully sent.
 * @return false if failed to sent.
 */
bool rtl8139_send_packet(const uint8 *data, uint16 length);

/**
 * @brief Receives a packet (polling mode - for manual retrieval if needed)
 *
 * @param buffer the received data
 * @param length the length of buffer
 * @return [true] Return yes if a packet was received
 * @return [false] Return false if a packet was not received
 */
bool rtl8139_receive_packet(uint8 *buffer, uint16 *length);

/**
 * @brief Interrupt handler for RTL8139 - processes incoming packets
 * Called from ISR context when hardware raises interrupt
 */
void rtl8139_interrupt_handler(void);

/**
 * @brief Enable interrupt-driven packet processing for RTL8139
 * Allows packets to be processed when hardware IRQ fires instead of polling
 */
void rtl8139_irq_enable(void);

/**
 * @brief Disable interrupt-driven packet processing
 */
void rtl8139_irq_disable(void);

/**
 * @brief Check if interrupt-driven mode is active
 */
bool rtl8139_is_irq_driven(void);

#endif
