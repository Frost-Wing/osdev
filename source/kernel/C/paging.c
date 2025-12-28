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

static void mark_page_used(uint64_t addr) {
    if (addr < MEMORY_START || addr >= MEMORY_END) return;
    size_t page_index = (addr - MEMORY_START) / PAGE_SIZE;
    size_t byte = page_index / 8;
    size_t bit  = page_index % 8;
    page_bitmap[byte] |= (1 << bit);
}

static void mark_page_free(uint64_t addr) {
    if (addr < MEMORY_START || addr >= MEMORY_END) return;
    size_t page_index = (addr - MEMORY_START) / PAGE_SIZE;
    size_t byte = page_index / 8;
    size_t bit  = page_index % 8;
    page_bitmap[byte] &= ~(1 << bit);
}

void initialize_page_bitmap(int64 kernel_start, int64 kernel_end) {
    info("Initializing paging", __FILE__);

    size_t physical_memory_size = 32 MiB;
    void* memory_block = kmalloc(physical_memory_size);
    if (!memory_block) {
        error("Failed to allocate physical memory block!", __FILE__);
        return;
    }

    memory_start = (uint64_t)memory_block;
    memory_end   = memory_start + physical_memory_size;
    amount_of_pages = physical_memory_size / PAGE_SIZE;

    size_t bitmap_size = amount_of_pages / 8;
    page_bitmap = (int8*)kmalloc(bitmap_size);
    if (!page_bitmap) {
        error("Failed to allocate page bitmap!", __FILE__);
        return;
    }

    memset(page_bitmap, 0, bitmap_size);

    // Reserve kernel memory
    for (uint64_t addr = (uint64_t)kernel_start; addr < (uint64_t)kernel_end; addr += PAGE_SIZE)
        mark_page_used(addr);

    // Reserve user code memory
    for (uint64_t addr = (uint64_t)user_code_start; addr < (uint64_t)user_code_end; addr += PAGE_SIZE)
        mark_page_used(addr);

    mm_print_out();

    done("Successfully initialized page bitmap", __FILE__);
}

void* allocate_page() {
    for (size_t i = 0; i < amount_of_pages; ++i) {
        size_t byte = i / 8;
        size_t bit  = i % 8;
        if (!(page_bitmap[byte] & (1 << bit))) {
            page_bitmap[byte] |= (1 << bit);
            return (void*)(memory_start + i * PAGE_SIZE);
        }
    }
    error("Out of physical pages!", __FILE__);
    return NULL;
}

void free_page(void* addr) {
    uint64_t aligned_addr = ((uint64_t)addr / PAGE_SIZE) * PAGE_SIZE;
    mark_page_free(aligned_addr);
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

void map_user_code() {
    uint64_t size = (uint64_t)user_code_end - (uint64_t)user_code_start;

    debug_printf("size of user code -> %z\n", size);
    debug_printf("user code start   -> %z\n", user_code_start);
    debug_printf("user code end     -> %z\n", user_code_end);

    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        void* kernel_va = allocate_page();
        uint64_t phys = (int64)virtual_to_physical((int64)kernel_va);

        if (!phys)
            error("page allocation failed", __FILE__);
        else
            info("page allocation is fine", __FILE__);
        
        uint64_t vaddr = USER_CODE_VADDR + off;
        map_user_page(vaddr, phys, USER_CODE_FLAGS);
        info("map_user_code: mapped executable user page", __FILE__);

        // Verify mapping
        uint64_t resolved = virtual_to_physical(vaddr) & ~0xFFF;
        if (resolved != (phys & ~0xFFF)) {
            error("map_user_code: VA->PA mismatch", __FILE__);
        } else {
            info("map_user_code: VA->PA matches", __FILE__);
        }

        // The code bytes to copy.
        uint64_t copy = (size - off >= PAGE_SIZE) ? PAGE_SIZE : (size - off);
        memcpy(kernel_va, user_code_start + off, copy);

        // Verify the copy of userland.
        if (memcmp(kernel_va, user_code_start + off, copy) != 0) {
            error("map_user_code: memcpy verification failed", __FILE__);
        } else {
            info("map_user_code: memcpy verification succeeded", __FILE__);
        }
    }
}
