/**
 * @file heap.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The code for Heap Memory
 * @version 0.1
 * @date 2023-11-10
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <heap.h>
#include <graphics.h>

// Define a block structure to manage memory allocation.
typedef struct heap_block {
    size_t size;
    struct heap_block* next;
} heap_block;

// Global pointer to the beginning of the free memory list.
static heap_block* free_list = null;

/**
 * @brief Initialize the memory allocator.
 */
void init_heap(int size) {
    info("Started initialization!", __FILE__);
    int64 holder[size/sizeof(int64)];
    // Initialize the free list with a single block representing the entire memory.
    free_list = (heap_block*)holder;
    free_list->size = size;
    free_list->next = null;
    print(yellow_color);
    printf("Given Heap size  (Bytes) : %d Bytes", size);
    printf("Holder Heap size (Bytes) : %d Bytes", sizeof(holder));
    printf("Given Heap size  (MB)    : %d MiB", size/(1024*1024));
    printf("Holder Heap size (MB)    : %d MiB", sizeof(holder)/(1024*1024));
    print(reset_color);
    debug_print(yellow_color);
    debug_printf("Given Heap size  (Bytes) : %u Bytes", size);
    debug_printf("Holder Heap size (Bytes) : %u Bytes", sizeof(holder));
    debug_printf("Given Heap size  (MB)    : %u MiB", size/(1024*1024));
    debug_printf("Holder Heap size (MB)    : %u MiB", sizeof(holder)/(1024*1024));
    print(reset_color);
    done("Completed successfully!", __FILE__);
}

/**
 * @brief Allocate memory of the specified size.
 * @param size The size of memory to allocate.
 * @return A pointer to the allocated memory, or null if allocation fails.
 */
 void* malloc(size_t size) {
    if (size == 0) {
        return null;
    }

    // Find a free block that is large enough to hold the requested memory.
    heap_block* prev = null;
    heap_block* curr = free_list;

    while (curr != NULL) {
        if (curr->size >= size) {
            // Remove the current block from the free list
            if (prev != NULL) {
                prev->next = curr->next;
            } else {
                free_list = curr->next;
            }

            // If the block is significantly larger, split it
            if (curr->size - size >= sizeof(heap_block)) {
                heap_block* new_block = (heap_block*)((char*)curr + size);
                new_block->size = curr->size - size;
                new_block->next = free_list;
                free_list = new_block;
            }

            debug_printf("Pointer location -> %u", (uint32_t)((char*)curr + sizeof(heap_block))); 
            return ((char*)curr) + sizeof(heap_block); 
        } else {
            warn("Block size is too small! Checking next block...", __FILE__);
        }

        prev = curr;
        curr = curr->next;
    }

    error("No suitable block found for allocation", __FILE__);
    meltdown_screen("No suitable block found for allocation of heap.", __FILE__, __LINE__, 0x0);
    hcf(); // it is better to handle it here rather than leaving it.
    return null;
}

/**
 * @brief Reallocate memory for the given pointer with the specified size.
 * @param ptr A pointer to the memory block to be reallocated.
 * @param size The new size of the memory block.
 * @return A pointer to the reallocated memory block, or null if reallocation fails.
 */
void* realloc(void* ptr, size_t size) {
    if (ptr == null) {
        // If the pointer is null, perform a malloc.
        return malloc(size);
    }

    if (size == 0) {
        // If the size is zero, perform a free.
        free(ptr);
        return null;
    }

    // Allocate a new block of the given size.
    void* new_ptr = malloc(size);

    if (new_ptr != null) {
        // Get the original block header from the old allocated memory.
        heap_block* old_block = (heap_block*)((char*)ptr - sizeof(heap_block));

        // Copy the data from the old block to the new block.
        size_t copy_size = (size < old_block->size) ? size : old_block->size;
        memcpy(new_ptr, ptr, copy_size);

        // Free the old memory block.
        free(ptr);
    }

    return new_ptr;
}

/**
 * @brief Free a previously allocated memory block.
 * @param ptr A pointer to the memory block to be freed.
 */
void free(void* ptr) {
    if (ptr == null) return;

    // Get the original Block header from the allocated memory.
    heap_block* block = (heap_block*)((char*)ptr - sizeof(heap_block));

    // Add the block to the free list.
    block->next = free_list;
    free_list = block;
    // done("Freed up memory!", __FILE__);
}

/**
 * @brief Clean up resources and release allocated memory.
 */
void cleanup_heap() {
    // Traverse the list of allocated memory blocks and free them.
    heap_block* current = free_list;
    while (current != null) {
        // Store the next block before freeing the current block.
        heap_block* next = current->next;
        free(current);
        current = next;
    }
    free_list = null;  // Reset the free_list to indicate no allocated memory remains.
    done("Heap memory flushed!", __FILE__);
}