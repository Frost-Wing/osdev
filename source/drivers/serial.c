/**
 * @file serial.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Serial output drivers for FrostWing (Focused for hardware debugging).
 * @version 0.1
 * @date 2023-10-31
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <drivers/serial.h>

/**
 * @brief The port address of non faulty serial COM
 * 
 */
int select = 0;

/**
 * @brief Finds the working serial COM port in the OS ranging from 1-8
 * 
 */
void probe_serial(){
    info("Probe started!", __FILE__);
    if(!init_serial(COM1)) select = COM1;
    else if(!init_serial(COM2)) select = COM2;
    else if(!init_serial(COM3)) select = COM3;
    else if(!init_serial(COM4)) select = COM4;
    else if(!init_serial(COM5)) select = COM5;
    else if(!init_serial(COM6)) select = COM6;
    else if(!init_serial(COM7)) select = COM7;
    else if(!init_serial(COM8)) select = COM8;
    else {
        warn("Probe didn't detect any serial ports connected.", __FILE__); 
        return;
    }
    done("Probe completed successfully!", __FILE__);
    printf("The working COM port is: %d", select);
}

/**
 * @brief Initialize serial at a specific port
 * 
 * @param port the serial port
 * @return int status code [0 - Success] [1 - Port Faulty] [2 - Disabled]
 */
int init_serial(int port) {
#if serial_mode
   outb(port + 1, 0x00);    // Disable all interrupts
   outb(port + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   outb(port + 0, 0x03);    // Set divisor to 3 (low  byte) 38400 baud
   outb(port + 1, 0x00);    // Set divisor to 3 (high byte) 38400 baud
   outb(port + 3, 0x03);    // 8 bits, no parity, one stop bit
   outb(port + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   outb(port + 4, 0x0B);    // IRQs enabled, RTS/DSR set
   outb(port + 4, 0x1E);    // Set in loopback mode, test the serial chip
   outb(port + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)
 
   // Check if serial is faulty (i.e: not same byte as sent)
   if(inb(port + 0) != 0xAE) {
      return 1;
   }
 
   // If serial is not faulty set it in normal operation mode
   // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
   outb(port + 4, 0x0F);
   return 0;
#else
    return 2;
#endif
}

/**
 * @brief Get the current status code to transmit the data
 * 
 * @return int status code
 */
int transmit_status() {
   return inb(select + 5) & 0x20;
}
 
 /**
  * @brief Put a character to a serial COM
  * 
  * @param a the character to be displayed
  */
void serial_putc(char a) {
    if(select == 0) return;
#if serial_mode
   while (transmit_status() == 0);
 
   outb(select, a);
#endif
}

/**
 * @brief Put a string to a serial COM
 * 
 * @param msg the text to be displayed
 */
void serial_print(const char* msg){
    while(*msg){
        serial_putc(*msg);
        msg++;
    }
}