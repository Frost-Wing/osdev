/**
 * @file ringbuffer.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The kernel-wide ring buffer mainly for kernel get char.
 * @version 0.1
 * @date 2026-03-20
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <basics.h>

/**
 * @struct ring_buffer_t
 * @brief Structure representing a ring buffer.
 */
typedef struct {
    uint8_t* buffer;     // Pointer to raw memory buffer
    size_t capacity;     // Max number of elements
    size_t elem_size;    // Size of each element in bytes 

    size_t head;         // Write position
    size_t tail;         // Read position 
    size_t count;        // Number of elements stored 
} ring_buffer_t;

/**
 * @brief Initialize a ring buffer.
 *
 * @param rb Pointer to ring buffer structure
 * @param buffer Pre-allocated memory buffer
 * @param capacity Number of elements buffer can hold
 * @param elem_size Size of each element in bytes
 */
void rb_init(ring_buffer_t* rb, void* buffer, size_t capacity, size_t elem_size);

/**
 * @brief Check if buffer is full.
 *
 * @param rb Pointer to ring buffer
 * @return 1 if full, 0 otherwise
 */
int rb_full(const ring_buffer_t* rb);

/**
 * @brief Check if buffer is empty.
 *
 * @param rb Pointer to ring buffer
 * @return 1 if empty, 0 otherwise
 */
int rb_empty(const ring_buffer_t* rb);

/**
 * @brief Push an element into the buffer.
 *
 * @param rb Pointer to ring buffer
 * @param data Pointer to element to insert
 * @return 0 on success, -1 if buffer is full
 */
int rb_push(ring_buffer_t* rb, const void* data);

/**
 * @brief Pop an element from the buffer.
 *
 * @param rb Pointer to ring buffer
 * @param out Pointer where popped element will be stored
 * @return 0 on success, -1 if buffer is empty
 */
int rb_pop(ring_buffer_t* rb, void* out);

/**
 * @brief Peek at the next element without removing it.
 *
 * @param rb Pointer to ring buffer
 * @param out Pointer where element will be copied
 * @return 0 on success, -1 if buffer is empty
 */
int rb_peek(const ring_buffer_t* rb, void* out);

/**
 * @brief Clear the buffer.
 *
 * @param rb Pointer to ring buffer
 */
void rb_clear(ring_buffer_t* rb);

/**
 * @brief Get number of elements currently stored.
 *
 * @param rb Pointer to ring buffer
 * @return Number of elements
 */
size_t rb_size(const ring_buffer_t* rb);

/**
 * @brief Get remaining capacity.
 *
 * @param rb Pointer to ring buffer
 * @return Remaining slots
 */
size_t rb_free(const ring_buffer_t* rb);

#endif