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

// Function to read from RTC
uint8_t rtc_read(uint8_t reg) {
    outb(RTC_PORT, reg);
    return inb(RTC_DATA);
}

// Function to initialize the RTC
void init_rtc() {
    // Disable NMI (Non-Maskable Interrupt) by setting bit 7 of register B
    outb(RTC_PORT, 0x8B);
    uint8_t prev = inb(RTC_DATA);
    outb(RTC_PORT, 0x8B);
    outb(RTC_DATA, prev | 0x40);
}

// Function to update the system time using the RTC
void update_system_time(uint8_t *second, uint8_t *minute, uint8_t *hour, uint8_t *day, uint8_t *month, uint8_t *year) {
    *second = rtc_read(RTC_SECONDS);
    *minute = rtc_read(RTC_MINUTES);
    *hour = rtc_read(RTC_HOURS);
    *day = rtc_read(RTC_DAY);
    *month = rtc_read(RTC_MONTH);
    *year = rtc_read(RTC_YEAR);
}

void display_time(){
    uint8_t second, minute, hour, day, month, year;
    update_system_time(&second, &minute, &hour, &day, &month, &year);
    // TODO: Print this
}