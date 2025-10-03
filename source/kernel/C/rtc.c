/**
 * @file rtc.c
 * @author Pradosh
 * @brief Real time clock code (safe & BCD aware)
 * @version 0.2
 * @date 2025-10-03
 */

#include <stdint.h>
#include <basics.h>
 
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
 
 // --- Helpers ---
uint8_t bcd_to_bin(uint8_t val) {
    return (val & 0x0F) + ((val >> 4) * 10);
}
 
uint8_t read_rtc_register(uint8_t reg) {
    outb(RTC_PORT, reg);
    return inb(RTC_DATA);
}
 
// Wait until RTC is not updating
void wait_rtc_update() {
    while (read_rtc_register(0x0A) & 0x80);
}
 
// Safe read of a register (avoid tick glitch)
uint8_t rtc_read_stable(uint8_t reg) {
    uint8_t last, val;
    do {
        last = read_rtc_register(reg);
        val = read_rtc_register(reg);
    } while (val != last);
    return val;
}
 
void init_rtc() {
    // Enable periodic interrupts if desired, not strictly necessary
    uint8_t prev = read_rtc_register(0x0B);
    outb(RTC_PORT, 0x0B);
    outb(RTC_DATA, prev | 0x40); // Set bit 6 = Update-Ended Interrupt Enable (optional)
}
 
 void update_system_time(uint8_t *second, uint8_t *minute, uint8_t *hour, uint8_t *day, uint8_t *month, uint16_t *year) {
    wait_rtc_update();
 
    uint8_t regB = rtc_read_stable(0x0B);
    int is_bcd = !(regB & 0x04);
    int is_24h = regB & 0x02;
 
    uint8_t sec   = rtc_read_stable(RTC_SECONDS);
    uint8_t min   = rtc_read_stable(RTC_MINUTES);
    uint8_t hr    = rtc_read_stable(RTC_HOURS);
    uint8_t day_r = rtc_read_stable(RTC_DAY);
    uint8_t mon   = rtc_read_stable(RTC_MONTH);
    uint16_t yr    = rtc_read_stable(RTC_YEAR);
    uint8_t cent  = rtc_read_stable(RTC_CENTURY); // optional
 
    if (is_bcd) {
        sec   = bcd_to_bin(sec);
        min   = bcd_to_bin(min);
        hr    = bcd_to_bin(hr & 0x7F);
        day_r = bcd_to_bin(day_r);
        mon   = bcd_to_bin(mon);
        yr    = bcd_to_bin(yr);
        cent  = bcd_to_bin(cent);
    }
 
    // Handle 12-hour mode
    if (!is_24h && (hr & 0x80)) {
         hr = ((hr & 0x7F) + 12) % 24;
    }
 
    *second = sec;
    *minute = min;
    *hour   = hr;
    *day    = day_r;
    *month  = mon;
    *year = yr;
}
 
void display_time() {
    int8 sec, min, hr, day, mon;
    int16 yr;
    update_system_time(&sec, &min, &hr, &day, &mon, &yr);
    printf("Time: %02d:%02d:%02d %02d/%02d/%04d\n", hr, min, sec, mon, day, yr);
}
 
void sleep(int seconds) {
    uint8_t start_sec, cur_sec;
    uint8_t start_min, cur_min;
    uint8_t start_hr, cur_hr;
    
    start_sec = rtc_read_stable(RTC_SECONDS);
    start_min = rtc_read_stable(RTC_MINUTES);
    start_hr  = rtc_read_stable(RTC_HOURS);

    int elapsed = 0;

    while (elapsed < seconds) {
        cur_sec = rtc_read_stable(RTC_SECONDS);
        cur_min = rtc_read_stable(RTC_MINUTES);
        cur_hr  = rtc_read_stable(RTC_HOURS);

        // Calculate elapsed seconds, handling rollover
        elapsed = (cur_hr - start_hr) * 3600 +
                  (cur_min - start_min) * 60 +
                  (cur_sec - start_sec);

        if (elapsed < 0) elapsed += 24*3600; // rollover to next day
    }
}
