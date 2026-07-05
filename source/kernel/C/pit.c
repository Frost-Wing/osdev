/**
 * @file pit.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The code for PIT
 * @version 0.1
 * @date 2023-12-27
 *
 * @copyright Copyright (c) Pradosh 2023
 *
 */
#include <multitasking.h>
#include <pit.h>
#include <scheduler/scheduler.h>

volatile uint64_t pit_ticks = 0;

#define pit_freq 100 // Hz

void process_pit(InterruptFrame *frame) {
    (void)frame;
    pit_ticks++;

    scheduler_wakeup(pit_ticks);
    scheduler_tick((trap_frame_t *)frame);

    outb(0x20, 0x20); // Notify the PIC that we've handled the interrupt
}

void init_pit(void) {
    LOG_SCOPE();
    uint32_t divisor = 1193180 / pit_freq; // PIT operates at 1193180 Hz

    outb(0x43, 0x36);                            // Command byte: Channel 0, lobyte/hibyte, mode 3 (square wave generator)
    outb(0x40, (uint8)(divisor & 0xFFU));        // Set low byte of divisor
    outb(0x40, (uint8)((divisor >> 8) & 0xFFU)); // Set high byte of divisor
}

void pit_sleep(uint32_t milliseconds) {
    uint32_t ms_per_tick = 1000U / pit_freq;
    uint64_t ticks_to_wait = ((uint64_t)milliseconds + ms_per_tick - 1) / ms_per_tick; // round up
    uint64_t target_ticks = pit_ticks + ticks_to_wait;
    while (pit_ticks < target_ticks) {
        asm volatile("hlt");
    }
}

uint64_t get_time_ms(void) {
    // pit_ticks increments at pit_freq (100 Hz) => each tick = 1000/pit_freq ms
    return pit_ticks * (1000ULL / pit_freq);
}