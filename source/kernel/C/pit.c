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
#include <pit.h>

volatile int pit_ticks = 0;

#define pit_freq 100 // Hz

void process_pit(InterruptFrame* frame) {
    pit_ticks++;
    outb(0x20, 0x20);  // Notify the PIC that we've handled the interrupt
}

void init_pit() {
    uint32_t divisor = 1193180 / pit_freq;  // PIT operates at 1193180 Hz

    outb(0x43, 0x36);  // Command byte: Channel 0, lobyte/hibyte, mode 3 (square wave generator)
    outb(0x40, divisor & 0xFF);  // Set low byte of divisor
    outb(0x40, (divisor >> 8) & 0xFF);  // Set high byte of divisor
}

void pit_sleep(uint32_t milliseconds) {
    uint32_t target_ticks = pit_ticks + (milliseconds / (1000 / pit_freq));

    while (pit_ticks < target_ticks) {
        
    }
}