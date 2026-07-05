/**
 * @file scheduler.h
 * @brief FrostWing round-robin scheduler.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <scheduler/process.h>
#include <stdbool.h>
#include <stdint.h>

void scheduler_init(void);

/* Ready queue */
void scheduler_add(process_t *proc);
void scheduler_remove(process_t *proc);

/* Current process */
process_t *scheduler_current(void);

/* Pick next runnable process */
process_t *scheduler_next(void);

/* Called from PIT */
void scheduler_tick(trap_frame_t *frame);

/* Explicit yield */
void scheduler_yield(void);

/* Sleep */
void scheduler_sleep(process_t *proc, uint64_t ticks);

/* Wake sleeping tasks */
void scheduler_wakeup(uint64_t now);

/* Context switch */
void scheduler_switch(process_t *next);

#endif