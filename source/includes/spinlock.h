#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <basics.h>

typedef struct {
    volatile uint32_t value;
} spinlock_t;

#define SPINLOCK_INITIALIZER {0}

void spinlock_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);

#endif
