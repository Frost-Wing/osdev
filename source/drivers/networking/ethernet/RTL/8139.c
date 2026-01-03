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

struct rtl8139* RTL8139 = NULL;

void read_mac_address() {
    for (int i = 0; i < 6; i++) {
        RTL8139->mac_address[i] = inb(RTL8139->io_base + RTL8139_REG_MAC + i);
    }
}

// Initialize RTL8139 NIC
void rtl8139_init(struct rtl8139* nic) {
    if(RTL8139->io_base == null || RTL8139->io_base == 0){
        warn("RTL8139 Card is not detected but tried to initialize it. Skipping...", __FILE__);
        return;
    }
    info("Initialization started!", __FILE__);
    // Reset the NIC
    outw(nic->io_base + RTL8139_REG_COMMAND, RTL8139_CMD_RESET);
    // while (inb(nic->io_base + RTL8139_REG_COMMAND) & RTL8139_CMD_RESET);

    sleep(1); // for safety

    // Initialize MAC address
    read_mac_address();

    printf("Mac Address : %x:%x:%x:%x:%x:%x", nic->mac_address[0], nic->mac_address[1], nic->mac_address[2], nic->mac_address[3], nic->mac_address[4], nic->mac_address[5]);

    // Enable receive and transmit
    outw(nic->io_base + RTL8139_REG_COMMAND, RTL8139_CMD_RX_ENABLE | RTL8139_CMD_TX_ENABLE);
    done("Successfully Initialized!", __FILE__);
}

// Transmit a packet
bool rtl8139_send_packet(const int8* data, int16 length) {
    if(RTL8139->io_base == null || RTL8139->io_base == 0){
        warn("RTL8139 Card is not detected but tried to send data to it. Skipping...", __FILE__);
        return no;
    }
    // Check if the NIC is ready for transmission (status checks)
    int16 status = inw(RTL8139->io_base + RTL8139_REG_TX_STATUS);
    if ((status & 0x8000) == 0) {
        return no; // Transmission is not ready
    }

    // Write the packet to the transmit buffer
    int16 tx_buffer_offset = status >> 11;
    int16 tx_buffer_address = RTL8139->io_base + RTL8139_REG_TX_ADDR + tx_buffer_offset;
    for (int16 i = 0; i < length; i++) {
        outb(tx_buffer_address, data[i]);
        tx_buffer_address++;
    }

    // Trigger transmission
    outw(RTL8139->io_base + RTL8139_REG_TX_STATUS, (tx_buffer_offset << 11) | length);

    return yes; // Return yes if transmission was successful, no otherwise
}

// Receives a packet
bool rtl8139_receive_packet(int8* buffer, int16* length) {
    if(RTL8139->io_base == null || RTL8139->io_base == 0){
        warn("RTL8139 Card is not detected but tried to receive data. Skipping...", __FILE__);
        return no;
    }
    int16 status = inw(RTL8139->io_base + RTL8139_REG_RX_BUFFER);
    if ((status & 0x01) == 0) {
        return no; // No packet available
    }

    // Copy the received packet to the buffer
    int16 rx_buffer_offset = status >> 1;
    int16 rx_buffer_address = RTL8139->io_base + RTL8139_REG_RX_BUFFER + rx_buffer_offset;
    *length = inw(rx_buffer_address);
    rx_buffer_address += 4; // Skip status and reserved fields
    for (int16 i = 0; i < *length; i++) {
        buffer[i] = inb(rx_buffer_address);
        rx_buffer_address++;
    }

    // Notify the NIC that the packet is read (update the RX buffer offset)
    outw(RTL8139->io_base + RTL8139_REG_RX_BUFFER, rx_buffer_offset);

    return yes; // Return yes if a packet was received, no otherwise
}