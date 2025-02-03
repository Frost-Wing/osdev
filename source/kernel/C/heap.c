/**
 * @file heap.c
 * @author Pradosh (pradoshgame@gmail.com) & levex@linux.com
 * @brief 
 * @version 0.1
 * @date 2025-02-01
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#define HEAP_H
#include <heap.h>
#include <stddef.h>
#include <graphics.h>
#include <memory2.h>

uint32_t last_alloc = 0;
uint32_t heap_end = 0;
uint32_t heap_begin = 0;
uint32_t pheap_begin = 0;
uint32_t pheap_end = 0;
uint8_t *pheap_desc = 0;
uint32_t memory_used = 0;

void mm_init(uint32_t kernel_end)
{
    info("Initializing heap.", __FILE__);
	last_alloc = kernel_end + 0x1000;
	heap_begin = last_alloc;
	pheap_end = 0x400000;
	pheap_begin = pheap_end - (MAX_PAGE_ALIGNED_ALLOCS * 4096);
	heap_end = pheap_begin;
	heap_begin = heap_end - heap_begin;
	pheap_desc = (uint8_t *)malloc(MAX_PAGE_ALIGNED_ALLOCS);
    done("Heap initialized.", __FILE__);
}

void mm_extend(uint32_t additional_size)
{
    if (additional_size <= 0){
        warn("mm_extend: Invalid size.", __FILE__);
        return;
    }

    info("Extending heap.", __FILE__);
    heap_end += additional_size;
    printf("Heap extended by %d bytes. New heap end: 0x%x", additional_size, heap_end);
    done("Heap extended.", __FILE__);
}

void mm_constrict(uint32_t removal_size)
{
    if (removal_size <= 0){
        warn("mm_extend: Invalid size.", __FILE__);
        return;
    }

    info("Constricting heap.", __FILE__);
    heap_end -= removal_size;
    printf("Heap constricting by %d bytes. New heap end: 0x%x", removal_size, heap_end);
    done("Heap Constricted.", __FILE__);
}

void mm_print_out()
{
    printf("%sMemory used :%s %d bytes", yellow_color, reset_color, memory_used);
    printf("%sMemory free :%s %d bytes", yellow_color, reset_color, heap_end - heap_begin - memory_used);
    printf("%sHeap size   :%s %d bytes", yellow_color, reset_color, heap_end - heap_begin);
}

void free(void *mem)
{
	if(mem == 0){
		warn("free: Cannot free null pointer.", __FILE__);
		return;
	}

	alloc_t *alloc = (mem - sizeof(alloc_t));
	memory_used -= alloc->size + sizeof(alloc_t);
	alloc->status = 0;
}

void pfree(void *mem)
{
	if(mem < pheap_begin || mem > pheap_end) return;
	/* Determine which page is it */
	uint32_t ad = (uint32_t)mem;
	ad -= pheap_begin;
	ad /= 4096;
	/* Now, ad has the id of the page */
	pheap_desc[ad] = 0;
	return;
}

char* pmalloc(size_t size)
{
	if(size <= 0){
		warn("pmalloc: Cannot allocate 0 bytes.", __FILE__);
		return 0;
	}

	for(int i = 0; i < MAX_PAGE_ALIGNED_ALLOCS; i++)
	{
		if(pheap_desc[i]) continue;
		pheap_desc[i] = 1;
		// printf("PAllocated from 0x%x to 0x%x\n", pheap_begin + i*4096, pheap_begin + (i+1)*4096);
		return (char *)(pheap_begin + i*4096);
	}
	warn("pmalloc: Failed to allocate memory! Out of memory.", __FILE__);
	return 0;
}

char* malloc(size_t size)
{
	if(size <= 0){
		warn("malloc: Cannot allocate 0 bytes.", __FILE__);
		return 0;
	}

	/* Loop through blocks and find a block sized the same or bigger */
	uint8_t *mem = (uint8_t *)heap_begin;
	while((uint32_t)mem < last_alloc)
	{
		alloc_t *a = (alloc_t *)mem;
		/* If the alloc has no size, we have reaced the end of allocation */
		//mprint("mem=0x%x a={.status=%d, .size=%d}\n", mem, a->status, a->size);
		if(!a->size)
			goto nalloc;
		/* If the alloc has a status of 1 (allocated), then add its size
		 * and the sizeof alloc_t to the memory and continue looking.
		 */
		if(a->status) {
			mem += a->size;
			mem += sizeof(alloc_t);
			mem += 4;
			continue;
		}
		/* If the is not allocated, and its size is bigger or equal to the
		 * requested size, then adjust its size, set status and return the location.
		 */
		if(a->size >= size)
		{
			/* Set to allocated */
			a->status = 1;

			//mprint("RE:Allocated %d bytes from 0x%x to 0x%x\n", size, mem + sizeof(alloc_t), mem + sizeof(alloc_t) + size);
			memset(mem + sizeof(alloc_t), 0, size);
			memory_used += size + sizeof(alloc_t);
			return (char *)(mem + sizeof(alloc_t));
		}
		/* If it isn't allocated, but the size is not good, then
		 * add its size and the sizeof alloc_t to the pointer and
		 * continue;
		 */
		mem += a->size;
		mem += sizeof(alloc_t);
		mem += 4;
	}

	nalloc:;
	if(last_alloc+size+sizeof(alloc_t) >= heap_end)
	{
        meltdown_screen("Heap out of memory!", __FILE__, __LINE__, 0, getCR2(), 0);
        hcf();
	}
	alloc_t *alloc = (alloc_t *)last_alloc;
	alloc->status = 1;
	alloc->size = size;

	last_alloc += size;
	last_alloc += sizeof(alloc_t);
	last_alloc += 4;

	// printf("Allocated %d bytes from  to 0x%x", size, (uint32_t)alloc + sizeof(alloc_t), last_alloc);
	memory_used += size + 4 + sizeof(alloc_t);
	memset((char *)((uint32_t)alloc + sizeof(alloc_t)), 0, size);
	return (char *)((uint32_t)alloc + sizeof(alloc_t));
/*
	char* ret = (char*)last_alloc;
	last_alloc += size;
	if(last_alloc >= heap_end)
	{
		panic("Cannot allocate %d bytes! Out of memory.\n", size);
	}
	printf("Allocated %d bytes from 0x%x to 0x%x", size, ret, last_alloc);
	return ret;*/
}

char* realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size); // If ptr is NULL, treat as malloc
    }

    if (size == 0) {
        free(ptr);         // If size is 0, treat as free
        return NULL;
    }

    alloc_t *old_alloc = (alloc_t *)((uint8_t *)ptr - sizeof(alloc_t));
    size_t old_size = old_alloc->size;

    if (size == old_size) {
        return ptr; // If size is the same, return the original pointer
    }

    char *new_ptr = malloc(size); // Allocate new memory

    if (!new_ptr) {
        return NULL; // Allocation failed
    }

    size_t copy_size = (size < old_size) ? size : old_size; // Copy the smaller of the two sizes
    memcpy(new_ptr, ptr, copy_size); // Copy the data

    free(ptr); // Free the old memory block

    return new_ptr;
}