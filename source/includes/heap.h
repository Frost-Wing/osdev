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
#ifdef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <basics.h>

typedef struct {
	uint8_t status;
	uint32_t size;
}alloc_t;

#define MAX_PAGE_ALIGNED_ALLOCS 32

extern void mm_init(uint32_t kernel_end);
extern void mm_extend(uint32_t additional_size);

extern void mm_print_out();

extern char* pmalloc(size_t size);
extern char* malloc(size_t size);
extern char* realloc(void *ptr, size_t size);
extern void free(void *mem);

#endif