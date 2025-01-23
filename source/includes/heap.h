/**
 * @file heap.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Header files for heap
 * @version 0.1
 * @date 2023-11-10
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stddef.h>
#include <basics.h>

/**
 * @brief Initialize the memory allocator.
 */
void init_heap(int size);

/**
 * @brief Allocate memory of the specified size.
 * @param size The size of memory to allocate.
 * @return A pointer to the allocated memory, or null if allocation fails.
 */
void* malloc(size_t size);

/**
 * @brief Reallocate memory for the given pointer with the specified size.
 * @param ptr A pointer to the memory block to be reallocated.
 * @param size The new size of the memory block.
 * @return A pointer to the reallocated memory block, or null if reallocation fails.
 */
void* realloc(void* ptr, size_t size);

/**
 * @brief Free a previously allocated memory block.
 * @param ptr A pointer to the memory block to be freed.
 */
void free(void* ptr);

/**
 * @brief Clean up resources and release allocated memory.
 */
void cleanup_heap();