/**
 * @file hal.c
 * @author Pradosh (pradoshgame@gmail.com) and GAMINGNOOB (https://github.com/GAMINGNOOBdev)
 * @brief Hardware Abstraction Layer (HAL) -> Source from GoGX OS
 * @version v1.0
 * @date 2022-08-03
 * 
 * @copyright Copyright Pradosh (c) 2022
 * 
 */

#include <hal.h>

/**
 * @brief Output a byte to the specified I/O port.
 *
 * This function sends a byte to the specified I/O port using inline assembly.
 *
 * @param port  The 16-bit I/O port number.
 * @param value The 8-bit value to be sent to the port.
 */
void outb(uint16_t port, uint8_t value){
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * @brief Read a byte from the specified I/O port.
 *
 * This function reads a byte from the specified I/O port using inline assembly.
 *
 * @param port The 16-bit I/O port number.
 * @return    The 8-bit value read from the port.
 */
uint8_t inb(uint16_t port){
    uint8_t returnVal;
    __asm__ volatile ("inb %1, %0" : "=a"(returnVal) : "Nd"(port));
    return returnVal;
}

/**
 * @brief Output a 16-bit value to the specified I/O port.
 *
 * This function sends a 16-bit value to the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @param data       The 16-bit value to be sent to the port.
 */
void outw(uint16_t portNumber, uint16_t data) {
    __asm__ volatile("outw %0, %1" : : "a"(data), "Nd"(portNumber));
}

/**
 * @brief Perform an I/O wait operation.
 *
 * This function performs an I/O wait operation using inline assembly.
 * It is used to add a small delay in I/O operations.
 */
void io_wait(){
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

/**
 * @brief Read a 16-bit value from the specified I/O port.
 *
 * This function reads a 16-bit value from the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @return           The 16-bit value read from the port.
 */
uint16_t inw(uint16_t portNumber) {
    uint16_t data;
    __asm__ volatile("inw %1, %0" : "=a"(data) : "Nd"(portNumber));
    return data;
}

/**
 * @brief Read a 32-bit value from the specified I/O port.
 *
 * This function reads a 32-bit value from the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @return           The 32-bit value read from the port.
 */
uint32_t inl(uint16_t portNumber) {
    uint32_t data;
    __asm__ volatile("inl %1, %0" : "=a"(data) : "Nd"(portNumber));
    return data;
}

/**
 * @brief Output a 32-bit value to the specified I/O port.
 *
 * This function sends a 32-bit value to the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @param data       The 32-bit value to be sent to the port.
 */
void outl(uint16_t portNumber, uint32_t data) {
    __asm__ volatile("outl %0, %1" : : "a"(data), "Nd"(portNumber));
}

