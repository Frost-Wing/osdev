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

/**
 * @brief Output a byte to the specified I/O port.
 *
 * This function sends a byte to the specified I/O port using inline assembly.
 *
 * @param port  The 16-bit I/O port number.
 * @param value The 8-bit value to be sent to the port.
 */
void outb(uint16_t port, uint8_t value);

/**
 * @brief Read a byte from the specified I/O port.
 *
 * This function reads a byte from the specified I/O port using inline assembly.
 *
 * @param port The 16-bit I/O port number.
 * @return    The 8-bit value read from the port.
 */
uint8_t inb(uint16_t port);

/**
 * @brief Output a 16-bit value to the specified I/O port.
 *
 * This function sends a 16-bit value to the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @param data       The 16-bit value to be sent to the port.
 */
void outw(uint16_t portNumber, uint16_t data);

/**
 * @brief Read a 16-bit value from the specified I/O port.
 *
 * This function reads a 16-bit value from the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @return           The 16-bit value read from the port.
 */
uint16_t inw(uint16_t portNumber);

/**
 * @brief Read a 32-bit value from the specified I/O port.
 *
 * This function reads a 32-bit value from the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @return           The 32-bit value read from the port.
 */
uint32_t inl(uint16_t portNumber);

/**
 * @brief Output a 32-bit value to the specified I/O port.
 *
 * This function sends a 32-bit value to the specified I/O port using inline assembly.
 *
 * @param portNumber The 16-bit I/O port number.
 * @param data       The 32-bit value to be sent to the port.
 */
void outl(uint16_t portNumber, uint32_t data);

/**
 * @brief Perform an I/O wait operation.
 *
 * This function performs an I/O wait operation using inline assembly.
 * It is used to add a small delay in I/O operations.
 */
void io_wait();