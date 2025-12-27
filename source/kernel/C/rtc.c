/**
 * @file rtc.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The basic/main RTC code for the OS.
 * @version 0.1
 * @date 2025-10-11
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <rtc.h>
 
 // --- Helpers ---
int8 bcd_to_bin(int8 val) {
    return (val & 0x0F) + ((val >> 4) * 10);
}
 
int8 read_rtc_register(int8 reg) {
    outb(RTC_PORT, reg);
    return inb(RTC_DATA);
}
 
// Wait until RTC is not updating
void wait_rtc_update() {
    while (read_rtc_register(0x0A) & 0x80);
}
 
// Safe read of a register (avoid tick glitch)
int8 rtc_read_stable(int8 reg) {
    int8 last, val;
    do {
        last = read_rtc_register(reg);
        val = read_rtc_register(reg);
    } while (val != last);
    return val;
}
 
void init_rtc() {
    info("Initializing RTC", __FILE__);
    // Enable periodic interrupts if desired, not strictly necessary
    int8 prev = read_rtc_register(0x0B);
    outb(RTC_PORT, 0x0B);
    outb(RTC_DATA, prev | 0x40); // Set bit 6 = Update-Ended Interrupt Enable (optional)
    done("Initialized RTC", __FILE__);
}
 
void update_system_time(int8 *second, int8 *minute, int8 *hour, int8 *day, int8 *month, int16 *year) {
    wait_rtc_update();
 
    int8 regB = rtc_read_stable(0x0B);
    int is_bcd = !(regB & 0x04);
    int is_24h = regB & 0x02;
 
    int8 sec   = rtc_read_stable(RTC_SECONDS);
    int8 min   = rtc_read_stable(RTC_MINUTES);
    int8 hr    = rtc_read_stable(RTC_HOURS);
    int8 day_r = rtc_read_stable(RTC_DAY);
    int8 mon   = rtc_read_stable(RTC_MONTH);
    int16 yr   = rtc_read_stable(RTC_YEAR);
    int8 cent  = rtc_read_stable(RTC_CENTURY); // optional
 
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
    printf("Time: %d:%d:%d %d/%d/%d", hr, min, sec, mon, day, yr);
}
 
void sleep(int seconds) {
    int8 start_sec, cur_sec;
    int8 start_min, cur_min;
    int8 start_hr, cur_hr;
    
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
