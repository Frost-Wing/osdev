/**
 * @file paging.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Contains code and definitons for Paging
 * @version 0.1
 * @date 2023-12-17
 *
 * @copyright Copyright (c) Pradosh 2023-2026
 *
 */
#ifndef PAGING_H
#define PAGING_H

#include <basics.h>
#include <limine.h>
#include <userland.h>

#define PAGE_SIZE 4096ULL

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4
#define PAGE_PWT 0x8
#define PAGE_PCD 0x10
#define PAGE_NX (1ULL << 63)

#define USER_CODE_FLAGS (PAGE_PRESENT | PAGE_USER | PAGE_RW)
#define USER_DATA_FLAGS (PAGE_PRESENT | PAGE_USER | PAGE_RW | PAGE_NX)

#define PAGE_PHYS_ADDR_MASK 0x000FFFFFFFFFF000ULL

extern struct limine_memmap_response *memmap;

/**
 * @brief Function to map userland pages
 *
 * @param virt Virtual memory address of user
 * @param phys Physical memory address of kernel's user code.
 * @param flags Permissions
 */
void map_user_page(uint64_t virt, uint64_t phys, uint64_t flags);

/** @brief Remove a user page mapping. */
void unmap_user_page(uint64_t virt);

/** @brief Set user-page write and execute permissions. */
bool paging_set_user_page_permissions(uint64_t virt, bool writable, bool executable);

/**
 * @brief Sets the HHDM offset used to access physical memory virtually.
 *
 * @param offset HHDM offset provided by Limine.
 */
void paging_set_hhdm_offset(uint64_t offset);

/** @brief Allocate one physical page and return its address. */
uintptr_t allocate_page(void);

/** @brief Allocate a number of physical pages. */
uintptr_t allocate_pages(size_t count);

/** @brief Allocate a contiguous run of physical pages. */
uintptr_t allocate_pages_contiguous(size_t count);

/** @brief Convert a physical address to its HHDM virtual address. */
void *paging_phys_to_virt(uintptr_t phys);

/** @brief Map a physical MMIO range into the HHDM with device cache attributes. */
bool paging_map_mmio(uintptr_t phys, size_t size);

/** @brief Return the page-table flags for a user virtual address. */
uint64_t paging_user_page_flags(uint64_t virt);

/** @brief Translate a virtual address to a physical address using the fast path. */
uint64_t fast_virt_to_phys(void *v);

/** @brief Translate a virtual address to a physical address. */
uint64_t virt_to_phys(void *v);

/** @brief Free a previously allocated physical page. */
void free_page(uintptr_t phys);

/** @brief Reserve a physical address range so the allocator will not use it. */
void paging_reserve_range(uintptr_t phys_start, uintptr_t phys_end);

#endif
