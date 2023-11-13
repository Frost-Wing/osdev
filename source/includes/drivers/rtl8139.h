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