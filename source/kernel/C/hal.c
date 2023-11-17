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
void outb(int16 port, int8 value){
    #if defined (__x86_64__)
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
    #endif
}

/**
 * @brief Read a byte from the specified I/O port.
 *
 * This function reads a byte from the specified I/O port using inline assembly.
 *
 * @param port The 16-bit I/O port number.
 * @return    The 8-bit value read from the port.
 */
int8 inb(int16 port){
    #if defined (__x86_64__)
    int8 returnVal;
    __asm__ volatile ("inb %1, %0" : "=a"(returnVal) : "Nd"(port));
    return returnVal;
    #endif
    
    return null;
}

/**
 * @brief Output a 16-bit value to the specified I/O port.
 *
 * This function sends a 16-bit value to the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @param data       The 16-bit value to be sent to the port.
 */
void outw(int16 portNumber, int16 data) {
    #if defined (__x86_64__)
    __asm__ volatile("outw %0, %1" : : "a"(data), "Nd"(portNumber));
    #endif
}

/**
 * @brief Perform an I/O wait operation.
 *
 * This function performs an I/O wait operation using inline assembly.
 * It is used to add a small delay in I/O operations.
 */
void io_wait(){
    #if defined (__x86_64__)
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
    #endif
}

/**
 * @brief Read a 16-bit value from the specified I/O port.
 *
 * This function reads a 16-bit value from the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @return           The 16-bit value read from the port.
 */
int16 inw(int16 portNumber) {
    #if defined (__x86_64__)
    int16 data;
    __asm__ volatile("inw %1, %0" : "=a"(data) : "Nd"(portNumber));
    return data;
    #endif

    return null;
}

/**
 * @brief Read a 32-bit value from the specified I/O port.
 *
 * This function reads a 32-bit value from the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @return           The 32-bit value read from the port.
 */
int32 inl(int16 portNumber) {
    #if defined (__x86_64__)
    int32 data;
    __asm__ volatile("inl %1, %0" : "=a"(data) : "Nd"(portNumber));
    return data;
    #endif

    return null;
}

/**
 * @brief Output a 32-bit value to the specified I/O port.
 *
 * This function sends a 32-bit value to the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @param data       The 32-bit value to be sent to the port.
 */
void outl(int16 portNumber, int32 data) {
    #if defined (__x86_64__)
    __asm__ volatile("outl %0, %1" : : "a"(data), "Nd"(portNumber));
    #endif
}

