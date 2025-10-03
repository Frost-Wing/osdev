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
#include <userland.h>

#define page_size 4096

#define memory_start 0x100000
#define memory_end   0x200000

#define amount_of_pages    ((memory_end - memory_start) / page_size)

#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2
#define PAGE_USER    0x4

#define PAGE_SIZE        0x1000      // 4 KB
#define USER_STACK_VADDR 0x70000000  // virtual top of user stack
#define USER_CODE_VADDR  0x40000000  // virtual address for user code
#define USER_STACK_SIZE  0x4000      // 16 KB stack


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

/**
 * @brief Function to map userland pages
 * 
 * @param virt Virtual memory address
 * @param phys Physical memory address
 */
void map_user_page(uint64_t virt, uint64_t phys);

/**
 * @brief Set the up physical memory for userland
 * 
 */
void setup_userland_memory();