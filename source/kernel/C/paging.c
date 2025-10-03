/**
 * @file paging.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The source for Paging
 * @version 0.1
 * @date 2023-12-17
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <paging.h>

int8 page_bitmap[amount_of_pages / 8];

void initialize_page_bitmap() {
    info("Initializing paging!", __FILE__);
    for (size_t i = 0; i < sizeof(page_bitmap) / sizeof(page_bitmap[0]); ++i) {
        page_bitmap[i] = 0;
    }
    done("Successfully initialized page bitmap!", __FILE__);
}

void* allocate_page() {
    for (size_t i = 0; i < amount_of_pages; ++i) {
        size_t byte_offset = i / 8;
        size_t bit_offset = i % 8;

        if (!(page_bitmap[byte_offset] & (1 << bit_offset))) {
            page_bitmap[byte_offset] |= (1 << bit_offset);

            return (void*)(memory_start + i * page_size);
        }
    }

    error("No free pages available!", __FILE__);
    return NULL;
}

void free_page(void* addr) {
    size_t page_index = ((int64)addr - memory_start) / page_size;

    size_t byte_offset = page_index / 8;
    size_t bit_offset = page_index % 8;

    page_bitmap[byte_offset] &= ~(1 << bit_offset);
}

static inline uint64_t get_kernel_pml4() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void map_user_page(uint64_t virt, uint64_t phys) {
    // Traverse or create PML4 -> PDPT -> PD -> PT
    uint64_t *pml4 = (uint64_t*)get_kernel_pml4(); // existing kernel PML4
    uint64_t *pdpt, *pd, *pt;

    // Indexes
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    // Create PDPT if missing
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        pdpt = (uint64_t*)allocate_page();
        memset(pdpt, 0, 0x1000);
        pml4[pml4_idx] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);
    }

    // Create PD if missing
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        pd = (uint64_t*)allocate_page();
        memset(pd, 0, 0x1000);
        pdpt[pdpt_idx] = (uint64_t)pd | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    }

    // Create PT if missing
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        pt = (uint64_t*)allocate_page();
        memset(pt, 0, 0x1000);
        pd[pd_idx] = (uint64_t)pt | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);
    }

    // Map the physical page
    pt[pt_idx] = phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;

    // Flush TLB for this page
    asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

void setup_userland_memory() {
    // Map multiple pages for stack (16 KB total)
    for(int i = 0; i < 4; i++) {
        map_user_page(USER_STACK_VADDR + i*PAGE_SIZE, (uint64_t)allocate_page());
    }

    // Map 4 KB for code
    map_user_page(USER_CODE_VADDR, (uint64_t)allocate_page());
    memcpy((void*)USER_CODE_VADDR, (void*)userland_main, 0x1000);

    // Map 4 KB for rodata / strings (for printf)
    map_user_page(USER_CODE_VADDR + 0x1000, (uint64_t)allocate_page());
}
