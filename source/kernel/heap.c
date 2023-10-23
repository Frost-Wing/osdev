#include <stddef.h>
#include <stdint.h>

// Define a structure for memory blocks
typedef struct {
    size_t size;
    uint8_t data[];
} MemoryBlock;

#define HEAP_START_ADDRESS 0x100000
#define INITIAL_HEAP_SIZE 65536

static uint8_t* heap_top = (uint8_t*)HEAP_START_ADDRESS;

/**
 * @brief Initializes or sets up heap
 * 
 */
void initialize_heap() {
    info("Initialization started!", __FILE__);
    heap_top = (uint8_t*)HEAP_START_ADDRESS;
    done("Successfully Initialized!", __FILE__);
}

/**
 * @brief Same as malloc in C, allocates memory to the heap
 * 
 * @param size 
 * @return void* 
 */
void* allocate(size_t size) {
    size_t total_size = size + sizeof(MemoryBlock);

    // Align the memory block to a 16-byte boundary
    total_size = (total_size + 0xF) & ~0xF;

    if (heap_top + total_size <= (uint8_t*)HEAP_START_ADDRESS + INITIAL_HEAP_SIZE) {
        MemoryBlock* block = (MemoryBlock*)heap_top;
        block->size = size;
        heap_top += total_size;
        return (void*)block->data;
    } else {
        // Handle out-of-memory condition
        return NULL;
    }
}

/**
 * @brief Same as free in C, deallocates memory from the heap
 * 
 * @param ptr 
 */
void deallocate(void* ptr) {
    if (ptr != NULL) {
        // Calculate the address of the memory block header
        MemoryBlock* block = (MemoryBlock*)((uint8_t*)ptr - sizeof(MemoryBlock));

        // Adjust the heap top pointer to release the memory
        heap_top = (uint8_t*)block;
    }
}