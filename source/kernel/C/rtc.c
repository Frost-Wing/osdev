/**
 * @file rtc.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Real time clock code.
 * @version 0.1
 * @date 2023-10-23
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */

#include <stdint.h>

// The I/O ports for RTC
#define RTC_PORT 0x70
#define RTC_DATA 0x71

// RTC Registers
#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS   0x04
#define RTC_DAY     0x07
#define RTC_MONTH   0x08
#define RTC_YEAR    0x09

/**
 * @brief Reads RTC for data.
 * 
 * @param reg 
 * @return uint8_t 
 */
uint8_t rtc_read(uint8_t reg) {
    outb(RTC_PORT, reg);
    return inb(RTC_DATA);
}

/**
 * @brief Initialises the RTC
 * 
 */
void init_rtc() {
    // Disable NMI (Non-Maskable Interrupt) by setting bit 7 of register B
    outb(RTC_PORT, 0x8B);
    uint8_t prev = inb(RTC_DATA);
    outb(RTC_PORT, 0x8B);
    outb(RTC_DATA, prev | 0x40);
}

/**
 * @brief Calls rtc_read() and gets the respected values below.
 * 
 * @param second 
 * @param minute 
 * @param hour 
 * @param day 
 * @param month 
 * @param year 
 */
void update_system_time(uint8_t *second, uint8_t *minute, uint8_t *hour, uint8_t *day, uint8_t *month, uint8_t *year) {
    *second = rtc_read(RTC_SECONDS);
    *minute = rtc_read(RTC_MINUTES);
    *hour = rtc_read(RTC_HOURS);
    *day = rtc_read(RTC_DAY);
    *month = rtc_read(RTC_MONTH);
    *year = rtc_read(RTC_YEAR);
}
/**
 * @brief Updates and prints the time to terminal.
 * 
 */
void display_time(){
    uint8_t second, minute, hour, day, month, year;
    update_system_time(&second, &minute, &hour, &day, &month, &year);
    // Depriciated
    // printf("Time: %d:%d:%d %d/%d/%d", hour, minute, second, month, day, year);
}

/**
 * @brief The time period where the computer stays idle.
 * 
 * @param seconds 
 */
void sleep(int seconds) {
    uint8_t start_second, start_minute, start_hour, start_day, start_month, start_year;
    update_system_time(&start_second, &start_minute, &start_hour, &start_day, &start_month, &start_year);

    while (1) {
        process_keyboard();
        
        uint8_t current_second, current_minute, current_hour, current_day, current_month, current_year;
        update_system_time(&current_second, &current_minute, &current_hour, &current_day, &current_month, &current_year);

        // Calculate the elapsed time in seconds
        int elapsed_seconds = (current_hour - start_hour) * 3600 +
            (current_minute - start_minute) * 60 +
            (current_second - start_second);

        if (elapsed_seconds >= seconds) {
            break;  // Exit the loop after the specified sleep duration
        }
    }
}

/**
 * @brief This is a function that is ran even when the sleep() function is called
 * 
 */
void process_keyboard(){
    int keyboard = inb(0x60);
        if(keyboard == 0x44){ // F10 Key
            shutdown();
        }
}