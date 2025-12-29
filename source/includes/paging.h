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
#ifndef PAGING_H
#define PAGING_H

#include <basics.h>
#include <userland.h>

#define PAGE_SIZE      4096ULL

#define PAGE_PRESENT  0x1
#define PAGE_RW       0x2
#define PAGE_USER     0x4
#define PAGE_NX       (1ULL << 63)

#define USER_CODE_FLAGS (PAGE_PRESENT | PAGE_USER)          // executable
#define USER_DATA_FLAGS (PAGE_PRESENT | PAGE_USER | PAGE_RW | PAGE_NX)

#define KERNEL_PHYS_OFFSET 0xFFFF800000000000ULL

/**
 * @brief Function to initialize the page bitmap
 * 
 */
void initialize_page_bitmap(int64 kernel_start, int64 kernel_end);

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
 * @param virt Virtual memory address of user
 * @param phys Physical memory address of kernel's user code.
 * @param flags Permissions
 */
void map_user_page(uint64_t virt, uint64_t phys, uint64_t flags);

/**
 * @brief Set the up physical memory for userland
 * 
 */
void setup_userland_memory();

/**
 * @brief Maps user code in paging.
 * 
 */
void map_user_code(uint64_t code_entry);

#endif