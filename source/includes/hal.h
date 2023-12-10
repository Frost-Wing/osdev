/**
 * @file hal.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Header file for Hardware Abstraction Layer -> Source from GoGX OS
 * @version v1.0
 * @date 2022-08-03
 * 
 * @copyright Copyright Pradosh (c) 2022
 * 
 */

#include <stdint.h>
#include <basics.h>

#define pic1_command 0x20
#define pic1_data 0x21
#define pic2_command 0xA0
#define pic2_data 0xA1

/**
 * @brief Initializes HAL by remapping the PIC interrupts to avoid conflicts
 * 
 */
void init_hardware_abstraction_layer();

/**
 * @brief Output a byte to the specified I/O port.
 *
 * This function sends a byte to the specified I/O port using inline assembly.
 *
 * @param port  The 16-bit I/O port number.
 * @param value The 8-bit value to be sent to the port.
 */
void outb(int16 port, int8 value);

/**
 * @brief Read a byte from the specified I/O port.
 *
 * This function reads a byte from the specified I/O port using inline assembly.
 *
 * @param port The 16-bit I/O port number.
 * @return    The 8-bit value read from the port.
 */
int8 inb(int16 port);

/**
 * @brief Output a 16-bit value to the specified I/O port.
 *
 * This function sends a 16-bit value to the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @param data       The 16-bit value to be sent to the port.
 */
void outw(int16 portNumber, int16 data);

/**
 * @brief Read a 16-bit value from the specified I/O port.
 *
 * This function reads a 16-bit value from the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @return           The 16-bit value read from the port.
 */
int16 inw(int16 portNumber);

/**
 * @brief Read a 32-bit value from the specified I/O port.
 *
 * This function reads a 32-bit value from the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @return           The 32-bit value read from the port.
 */
int32 inl(int16 portNumber);

/**
 * @brief Output a 32-bit value to the specified I/O port.
 *
 * This function sends a 32-bit value to the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @param data       The 32-bit value to be sent to the port.
 */
void outl(int16 portNumber, int32 data);

/**
 * @brief Perform an I/O wait operation.
 *
 * This function performs an I/O wait operation using inline assembly.
 * It is used to add a small delay in I/O operations.
 */
void io_wait();