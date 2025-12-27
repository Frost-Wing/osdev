/**
 * @file rtc.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Real Time Clock (RTC) code header.
 * @version 0.1
 * @date 2025-10-11
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#ifndef RTC_H
#define RTC_H

#include <basics.h>
#include <graphics.h>

// I/O ports for RTC
#define RTC_PORT 0x70
#define RTC_DATA 0x71
 
// RTC Registers
#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS   0x04
#define RTC_DAY     0x07
#define RTC_MONTH   0x08
#define RTC_YEAR    0x09
#define RTC_CENTURY 0x32  // Optional, not all BIOSes use it

/**
 * @brief Converts BDC to binary.
 * 
 * @param val The BCD value.
 * @return int8 
 */
int8 bcd_to_bin(int8 val);

/**
 * @brief Reads the value from an RTC register.
 * 
 * @param reg The register number
 * @return int8 Value of the Register
 */
int8 read_rtc_register(int8 reg);

/**
 * @brief Wait till RTC is responding.
 * 
 */
void wait_rtc_update();

/**
 * @brief Reads from a register without tick glitch (more stable)
 * 
 * @param reg 
 * @return int8 
 */
int8 rtc_read_stable(int8 reg);

/**
 * @brief Initializes the main RTC for use.
 * 
 */
void init_rtc();

/**
 * @brief Updated the given variable with system time.
 * 
 * @param second 
 * @param minute 
 * @param hour 
 * @param day 
 * @param month 
 * @param year 
 */
void update_system_time(int8 *second, int8 *minute, int8 *hour, int8 *day, int8 *month, int16 *year);

/**
 * @brief Displays time in an neat format.
 * 
 */
void display_time();

/**
 * @brief Pauses the OS for set seconds. Use PIT for accuracy.
 * 
 * @param seconds 
 */
void sleep(int seconds);

#endif