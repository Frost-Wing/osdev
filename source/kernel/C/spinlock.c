#include <spinlock.h>

void spinlock_lock(spinlock_t *lock) {
    while (__atomic_test_and_set(&lock->value, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&lock->value, __ATOMIC_RELAXED))
            __asm__ volatile("pause");
    }
}

void spinlock_unlock(spinlock_t *lock) {
    __atomic_clear(&lock->value, __ATOMIC_RELEASE);
}
