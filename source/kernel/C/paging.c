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

uint64_t memory_start;
uint64_t memory_end;
size_t   amount_of_pages;
int8*    page_bitmap;

extern uint8_t user_code_start[];
extern uint8_t user_code_end[];

struct limine_memmap_response *memmap;
static uintptr_t bump_ptr = 0;

uintptr_t allocate_page() {
    if(!memmap){
        error("Limine failed to give the memory map", __FILE__);
        hcf2();
    }

    if (!bump_ptr) {
        for (uint64_t i = 0; i < memmap->entry_count; i++) {
            struct limine_memmap_entry *e = memmap->entries[i];
            if (e->type != LIMINE_MEMMAP_USABLE) continue;
            bump_ptr = (e->base + PAGE_SIZE - 1) & ~0xFFFULL;
            break;
        }
    }

    uintptr_t page = bump_ptr;
    bump_ptr += PAGE_SIZE;
    return page;
}

static inline uint64_t get_kernel_pml4() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

uint64_t virtual_to_physical(uint64_t virt) {
    uint64_t *pml4 = (uint64_t*)get_kernel_pml4();
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;
    uint64_t offset   = virt & 0xFFF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    uint64_t *pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    uint64_t *pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);

    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    uint64_t *pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);

    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;

    uint64_t phys = (pt[pt_idx] & ~0xFFF) | offset;
    return phys;
}

void map_user_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    // Traverse or create PML4 -> PDPT -> PD -> PT
    uint64_t *pml4 = (uint64_t*)get_kernel_pml4(); // kernel PML4
    uint64_t *pdpt, *pd, *pt;

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

    pt[pt_idx] = phys | flags;

    // Flush TLB
    asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}
