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
#pragma once

#include <stddef.h>
#include <basics.h>

#define HEAP_SAFE_BEGIN 0x200000 // 2 MB mark, just after kernel & stack
#define HEAP_SAFE_SIZE  0x100000 // 1 MB heap for testing

/**
 * @brief Function to initlialize heap.
 * 
 * @param kernel_end The position in memory where kernel ends.
 */
extern void mm_init(uintptr_t kernel_end);

/**
 * @brief Function to extend available heap.
 * 
 * @param additional_size Additional size to be added.
 */
extern void mm_extend(uint64_t additional_size);

/**
 * @brief Function to constrict available heap.
 * 
 * @param removal_size Amount of size needed to be reduced.
 */
extern void mm_constrict(uint64_t removal_size);

/**
 * @brief Prints out heap information.
 * 
 */
extern void mm_print_out();

/**
 * @brief The main memory alloc function.
 * 
 * @param size Amount of size needed to be allocated.
 * @return void* Location in memory.
 */
extern void* kmalloc(size_t size);

/**
 * @brief The main memory realloc function.
 * 
 * @param size Amount of size needed to be extened.
 * @return void* Location in memory.
 */
extern void* krealloc(void* ptr, size_t size);

/**
 * @brief The main free function for memory allocation.
 * 
 * @param ptr Location in memory.
 */
extern void kfree(void *ptr);