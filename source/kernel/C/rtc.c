/**
 * @file rtc.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The basic/main RTC code for the OS.
 * @version 0.2
 * @date 2025-10-11
 *
 * @copyright Copyright (c) Pradosh 2025
 *
 */

#include <graphics.h>
#include <rtc.h>
#include <basics.h>

// --- Helpers ---
uint8 bcd_to_bin(uint8 val) {
    return (uint8)((val & 0x0FU) + (((val >> 4U) & 0x0FU) * 10U));
}

uint8 read_rtc_register(uint8 reg) {
    outb(RTC_PORT, reg);
    return inb(RTC_DATA);
}

// Wait until RTC is not updating
void wait_rtc_update(void) {
    while (read_rtc_register(0x0A) & 0x80)
        ;
}

// Safe read of a register (avoid tick glitch)
uint8 rtc_read_stable(uint8 reg) {
    uint8 last, val;
    do {
        last = read_rtc_register(reg);
        val = read_rtc_register(reg);
    } while (val != last);
    return val;
}

// Convert a raw RTC register value to binary, given the mode flags.
// Does NOT touch the PM bit (0x80) of an hour value — caller must
// extract that before calling this on an hour register.
static uint8 rtc_norm(uint8 val, int is_bcd) {
    return is_bcd ? bcd_to_bin(val) : val;
}

void init_rtc(void) {
    LOG_SCOPE();
    info("Initializing RTC", __FILE__);
    // NOTE: we deliberately do NOT enable any RTC interrupt (PIE/AIE/UIE)
    // here. Enabling one without an IRQ8 handler that reads Register C
    // to acknowledge it will leave the RTC's interrupt flag latched and
    // stop it delivering further interrupts. Wire this up only once an
    // IRQ8 handler exists.
    done("Initialized RTC", __FILE__);
}

void update_system_time(uint8 *second, uint8 *minute, uint8 *hour, uint8 *day, uint8 *month, uint16 *year) {
    wait_rtc_update();

    uint8 regB = rtc_read_stable(0x0B);
    int is_bcd = !(regB & 0x04);
    int is_24h = regB & 0x02;

    uint8 sec   = rtc_read_stable(RTC_SECONDS);
    uint8 min   = rtc_read_stable(RTC_MINUTES);
    uint8 hr    = rtc_read_stable(RTC_HOURS);
    uint8 day_r = rtc_read_stable(RTC_DAY);
    uint8 mon   = rtc_read_stable(RTC_MONTH);
    uint8 yr    = rtc_read_stable(RTC_YEAR);
    uint8 cent  = rtc_read_stable(RTC_CENTURY); // optional

    // Capture PM flag BEFORE it gets masked off by BCD conversion.
    int pm = hr & 0x80;
    hr &= 0x7F;

    sec   = rtc_norm(sec, is_bcd);
    min   = rtc_norm(min, is_bcd);
    hr    = rtc_norm(hr, is_bcd);
    day_r = rtc_norm(day_r, is_bcd);
    mon   = rtc_norm(mon, is_bcd);
    yr    = rtc_norm(yr, is_bcd);
    cent  = rtc_norm(cent, is_bcd);

    // Handle 12-hour mode using the PM flag captured earlier.
    if (!is_24h && pm) {
        hr = (uint8)(((hr % 12U) + 12U));
    } else if (!is_24h) {
        hr = (uint8)(hr % 12U); // 12-hour AM: 12 -> 0
    }

    *second = sec;
    *minute = min;
    *hour   = hr;
    *day    = day_r;
    *month  = mon;

    // Build a full 4-digit year. Century register isn't present/reliable
    // on all hardware, so fall back to 2000+yr if it looks bogus.
    uint16 full_year;
    if (cent == 0 || cent > 99) {
        full_year = (uint16)(2000U + yr);
    } else {
        full_year = (uint16)((uint16)cent * 100U + yr);
    }
    *year = full_year;
}

void display_time(void) {
    LOG_SCOPE();
    uint8 sec, min, hr, day, mon;
    uint16 yr;
    update_system_time(&sec, &min, &hr, &day, &mon, &yr);

    const char *ampm = (hr >= 12) ? "PM" : "AM";
    uint8 hr12 = hr % 12;
    if (hr12 == 0)
        hr12 = 12; // 0 and 12 both display as 12

    info("Time: %d:%d:%d %s %d/%d/%d", __FILE__, hr12, min, sec, ampm, day, mon, yr);
}

// Read the current time as total seconds-of-day, in binary (not BCD).
static uint32 rtc_seconds_of_day(int is_bcd) {
    uint8 sec = rtc_norm(rtc_read_stable(RTC_SECONDS), is_bcd);
    uint8 min = rtc_norm(rtc_read_stable(RTC_MINUTES), is_bcd);
    uint8 hr  = rtc_norm((uint8)(rtc_read_stable(RTC_HOURS) & 0x7F), is_bcd);
    return (uint32)hr * 3600U + (uint32)min * 60U + (uint32)sec;
}

void sleep(int seconds) {
    uint8 regB = rtc_read_stable(0x0B);
    int is_bcd = !(regB & 0x04);

    uint32 start = rtc_seconds_of_day(is_bcd);
    int elapsed = 0;

    while (elapsed < seconds) {
        wait_rtc_update();
        uint32 cur = rtc_seconds_of_day(is_bcd);

        uint32 diff = (uint32)cur - (uint32)start;
        if (diff < 0)
            diff += 24 * 3600; // rolled over midnight

        elapsed = diff;
    }
}