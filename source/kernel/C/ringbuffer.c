/**
 * @file ringbuffer.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The kernel-wide ring buffer mainly for kernel get char.
 * @version 0.1
 * @date 2026-03-20
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#include <ringbuffer.h>
#include <memory.h>

void rb_init(ring_buffer_t* rb, void* buffer, size_t capacity, size_t elem_size)
{
    rb->buffer = (uint8_t*)buffer;
    rb->capacity = capacity;
    rb->elem_size = elem_size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

int rb_full(const ring_buffer_t* rb)
{
    return rb->count == rb->capacity;
}

int rb_empty(const ring_buffer_t* rb)
{
    return rb->count == 0;
}

int rb_push(ring_buffer_t* rb, const void* data)
{
    if (rb_full(rb))
        return -1;

    uint8_t* dest = rb->buffer + (rb->head * rb->elem_size);
    memcpy(dest, data, rb->elem_size);

    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;

    return 0;
}

int rb_pop(ring_buffer_t* rb, void* out)
{
    if (rb_empty(rb))
        return -1;

    uint8_t* src = rb->buffer + (rb->tail * rb->elem_size);
    memcpy(out, src, rb->elem_size);

    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;

    return 0;
}

int rb_peek(const ring_buffer_t* rb, void* out)
{
    if (rb_empty(rb))
        return -1;

    uint8_t* src = rb->buffer + (rb->tail * rb->elem_size);
    memcpy(out, src, rb->elem_size);

    return 0;
}

void rb_clear(ring_buffer_t* rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

size_t rb_size(const ring_buffer_t* rb)
{
    return rb->count;
}

size_t rb_free(const ring_buffer_t* rb)
{
    return rb->capacity - rb->count;
}