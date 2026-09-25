/**
 * @file ringbuffer.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The kernel-wide ring buffer mainly for kernel get char & klog.
 * @version 0.1
 * @date 2026-03-20
 *
 * @copyright Copyright (c) Pradosh 2026
 *
 */
#include <memory.h>
#include <ringbuffer.h>

void rb_init(ring_buffer_t *rb, void *buffer, size_t capacity, size_t elem_size) {
    rb->buffer = (uint8_t *)buffer;
    rb->capacity = capacity;
    rb->elem_size = elem_size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->lock.value = 0;
}

int rb_full(const ring_buffer_t *rb) {
    spinlock_lock((spinlock_t *)&rb->lock);
    int full = rb->count == rb->capacity;
    spinlock_unlock((spinlock_t *)&rb->lock);
    return full;
}

int rb_empty(const ring_buffer_t *rb) {
    spinlock_lock((spinlock_t *)&rb->lock);
    int empty = rb->count == 0;
    spinlock_unlock((spinlock_t *)&rb->lock);
    return empty;
}

int rb_push(ring_buffer_t *rb, const void *data) {
    spinlock_lock(&rb->lock);
    if (rb->count == rb->capacity) {
        spinlock_unlock(&rb->lock);
        return -1;
    }

    uint8_t *dest = rb->buffer + (rb->head * rb->elem_size);
    memcpy(dest, data, rb->elem_size);

    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;

    spinlock_unlock(&rb->lock);
    return 0;
}

int rb_push_overwrite(ring_buffer_t *rb, const void *data) {
    spinlock_lock(&rb->lock);
    if (rb->count == rb->capacity) {
        rb->tail = (rb->tail + 1) % rb->capacity;
        rb->count--;
    }

    uint8_t *dest = rb->buffer + (rb->head * rb->elem_size);
    memcpy(dest, data, rb->elem_size);

    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;

    spinlock_unlock(&rb->lock);
    return 0;
}

int rb_pop(ring_buffer_t *rb, void *out) {
    spinlock_lock(&rb->lock);
    if (rb->count == 0) {
        spinlock_unlock(&rb->lock);
        return -1;
    }

    uint8_t *src = rb->buffer + (rb->tail * rb->elem_size);
    memcpy(out, src, rb->elem_size);

    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;

    spinlock_unlock(&rb->lock);
    return 0;
}

int rb_peek(const ring_buffer_t *rb, void *out) {
    spinlock_lock((spinlock_t *)&rb->lock);
    if (rb->count == 0) {
        spinlock_unlock((spinlock_t *)&rb->lock);
        return -1;
    }

    uint8_t *src = rb->buffer + (rb->tail * rb->elem_size);
    memcpy(out, src, rb->elem_size);

    spinlock_unlock((spinlock_t *)&rb->lock);
    return 0;
}

void rb_clear(ring_buffer_t *rb) {
    spinlock_lock(&rb->lock);
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    spinlock_unlock(&rb->lock);
}

size_t rb_size(const ring_buffer_t *rb) {
    spinlock_lock((spinlock_t *)&rb->lock);
    size_t size = rb->count;
    spinlock_unlock((spinlock_t *)&rb->lock);
    return size;
}

size_t rb_free(const ring_buffer_t *rb) {
    spinlock_lock((spinlock_t *)&rb->lock);
    size_t free = rb->capacity - rb->count;
    spinlock_unlock((spinlock_t *)&rb->lock);
    return free;
}
