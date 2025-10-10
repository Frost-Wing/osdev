/**
 * @file serial.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Header files for serial drivers
 * @version 0.1
 * @date 2023-10-31
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#ifndef SERIAL_H
#define SERIAL_H

#include <basics.h>
#include <graphics.h>
#include <hal.h>

#define serial_mode 1

// The list of All Serial COM Ports
#define COM1 1016
#define COM2 0760
#define COM3 1000
#define COM4 0744
#define COM5 1528
#define COM6 1272
#define COM7 1512
#define COM8 1256

/**
 * @brief The port address of non faulty serial COM
 * 
 */
extern int select;

/**
 * @brief Finds the working serial COM port in the OS ranging from 1-8
 * 
 */
void probe_serial();

/**
 * @brief Initialize serial at a specific port
 * 
 * @param port the serial port
 * @return int status code [0 - Success] [1 - Port Faulty] [2 - Disabled]
 */
int init_serial(int port);

/**
 * @brief Get the current status code to transmit the data
 * 
 * @return int status code
 */
int transmit_status();

/**
 * @brief Put a character to a serial COM
 * 
 * @param a the character to be displayed
 */
void serial_putc(char a);

/**
 * @brief Put a string to a serial COM
 * 
 * @param msg the text to be displayed
 */
void serial_print(const char* msg);

#endif