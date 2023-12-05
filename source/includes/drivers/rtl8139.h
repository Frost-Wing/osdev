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
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <basics.h>
#include <hal.h>

// RTL8139 registers
#define RTL8139_REG_MAC         0x00
#define RTL8139_REG_TX_STATUS   0x10
#define RTL8139_REG_TX_ADDR     0x20
#define RTL8139_REG_RX_BUFFER   0x30
#define RTL8139_REG_COMMAND     0x37

// RTL8139 commands
#define RTL8139_CMD_RESET       0x10
#define RTL8139_CMD_RX_ENABLE   0x09
#define RTL8139_CMD_TX_ENABLE   0x04

// RTL8139 packet buffer size
#define RTL8139_BUFFER_SIZE     8192

// RTL8139 PCI addresses
#define RTL8139_IOADDR1     0x10
#define RTL8139_CMD         0x37

struct rtl8139 {
    int16 io_base;
    int8 mac_address[6];
};

extern struct rtl8139* RTL8139;

/**
 * @brief Function to read the MAC address from EEPROM
 * 
 * @param nic 
 */
void read_mac_address(struct rtl8139* nic);

/**
 * @brief Initialize RTL8139 NIC
 * 
 * @param nic the pointer to RTL structure
 */
void rtl8139_init(struct rtl8139* nic);

/**
 * @brief Transmit a packet from the RTL8139
 * 
 * @param nic 
 * @param data 
 * @param length 
 * @return true if successfully sent.
 * @return false if failed to sent.
 */
bool rtl8139_send_packet(struct rtl8139* nic, const int8* data, int16 length);

/**
 * @brief Receives a packet
 * 
 * @param nic the pointer to RTL structure
 * @param buffer the received data
 * @param length the length of buffer
 * @return [true] Return yes if a packet was received
 * @return [false] Return false if a packet was not received
 */
bool rtl8139_receive_packet(struct rtl8139* nic, int8* buffer, int16* length);