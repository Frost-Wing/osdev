/**
 * @file spinlock.h
 * @brief Spinlock type and synchronization operations.
 */
#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <basics.h>

typedef struct {
    volatile uint32_t value;
} spinlock_t;

#define SPINLOCK_INITIALIZER {0}

/**
 * @brief Acquire a spinlock, waiting until it becomes available.
 * @param lock Lock to acquire.
 */
void spinlock_lock(spinlock_t *lock);

/**
 * @brief Release a spinlock.
 * @param lock Lock to release.
 */
void spinlock_unlock(spinlock_t *lock);

#endif
