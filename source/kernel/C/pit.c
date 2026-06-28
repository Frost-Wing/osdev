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
#include <multitasking.h>

volatile uint64_t pit_ticks = 0;

#define pit_freq 100 // Hz

void process_pit(InterruptFrame* frame) {
    (void)frame;
    pit_ticks++;
    outb(0x20, 0x20);  // Notify the PIC that we've handled the interrupt
}

void init_pit(void) {
    uint32_t divisor = 1193180 / pit_freq;  // PIT operates at 1193180 Hz

    outb(0x43, 0x36);  // Command byte: Channel 0, lobyte/hibyte, mode 3 (square wave generator)
    outb(0x40, (int8)(divisor & 0xFFU));  // Set low byte of divisor
    outb(0x40, (int8)((divisor >> 8) & 0xFFU));  // Set high byte of divisor
}

void pit_sleep(uint32_t milliseconds) {
    uint64_t target_ticks = pit_ticks + (uint64_t)(milliseconds / (1000U / pit_freq));

    while (pit_ticks < target_ticks) {
        asm volatile("hlt");
    }
}