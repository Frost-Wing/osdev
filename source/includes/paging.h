/**
 * @file paging.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Contains code and definitons for Paging
 * @version 0.1
 * @date 2023-12-17
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>

#define page_size 4096

#define memory_start 0x100000
#define memory_end   0x200000

#define amount_of_pages    ((memory_end - memory_start) / page_size)

/**
 * @brief Bitmap to keep track of page allocation status.
 * 
 */
extern int8 page_bitmap[];

/**
 * @brief Function to initialize the page bitmap
 * 
 */
void initialize_page_bitmap();

/**
 * @brief Function to allocate a page.
 * 
 * @return Pointer to the allocated page's memory address
 */
void* allocate_page();

/**
 * @brief Function to free a page
 * 
 * @param addr Address of the allocated page.
 */
void free_page(void* addr);