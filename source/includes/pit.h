/**
 * @file pit.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Headers for PIT
 * @version 0.1
 * @date 2023-12-27
 *
 * @copyright Copyright (c) Pradosh 2023-2026
 *
 */
#ifndef PIT_H
#define PIT_H

#include <basics.h>
#include <hal.h>
#include <isr.h>
#include <stdint.h>

/* Channel 0 is programmed at this frequency by init_pit(). */
#define PIT_TICKS_PER_SECOND 100U

extern volatile uint64_t pit_ticks;

/**
 * @brief Interrupt handlers for PIT
 *
 * @param frame
 */
void process_pit(InterruptFrame *frame);

/**
 * @brief Initialize the programmable interval timer.
 */
void init_pit(void);

/**
 * @brief Wait for at least the specified number of milliseconds.
 *
 * @param milliseconds Duration to wait.
 */
void pit_sleep(uint32_t milliseconds);

/**
 * @brief Return elapsed time based on PIT ticks.
 *
 * @return Elapsed milliseconds since the PIT was initialized.
 */
uint64_t get_time_ms(void);

#endif // PIT_H
