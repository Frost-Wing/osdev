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
    printf("Given Heap size  (Bytes) : %d", size);
    printf("Holder Heap size (Bytes) : %d", sizeof(holder));
    printf("Given Heap size  (MB)    : %dMB", size/(1024*1024));
    printf("Holder Heap size (MB)    : %dMB", sizeof(holder)/(1024*1024));
    print(reset_color);
    done("Completed successfully!", __FILE__);
}

/**
 * @brief Allocate memory of the specified size.
 * @param size The size of memory to allocate.
 * @return A pointer to the allocated memory, or null if allocation fails.
 */
void* malloc(size_t size) {
    if (size == 0) return null;

    // Find a free block that is large enough to hold the requested memory.
    int64 holder[size/sizeof(int64)];
    heap_block* prev = (heap_block*)holder;
    heap_block* curr = free_list;
    while (curr != null) {
        if (curr->size >= size) {
            if (prev != null) {
                prev->next = curr->next;
            } else {
                free_list = curr->next;
            }

            done("Found a free pointer! returning the pointer back...", __FILE__);
            // Return a pointer to the allocated memory (skip the Block header).
            return ((char*)curr) + sizeof(heap_block);
        }else{
            warn("Block size is too small! Checking next block...", __FILE__);
        }
        prev = curr;
        curr = curr->next;
    }

    error("No suitable block found for allocation", __FILE__);
    sleep(5);
    // meltdown_screen("No suitable block found for allocation of heap.", __FILE__, __LINE__, 0x0);
    // hcf(); // it is better to handle it here rather than leaving it.
    return null;
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
void flush_heap() {
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