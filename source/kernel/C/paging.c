/**
 * @file paging.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The source for Paging
 * @version 0.3
 * @date 2023-12-17
 *
 * @copyright Copyright (c) Pradosh 2023-2026
 *
 */
#include <cc-asm.h>
#include <graphics.h>
#include <heap.h>
#include <memory.h>
#include <paging.h>

/* Catches double frees / frees of foreign pages. Costs a 2 MiB bitmap. */
#define PAGING_DEBUG

uint64_t memory_start;
uint64_t memory_end;
size_t amount_of_pages;
uint8 *page_bitmap;

extern uint8_t user_code_start[];
extern uint8_t user_code_end[];

struct limine_memmap_response *memmap;
static uintptr_t bump_ptr = 0;
static uintptr_t bump_end = 0;
static uintptr_t free_list_head = 0;
uint64_t hhdm_offset = 0;

/* ------------------------------------------------- reserved phys ranges --- */

#define MAX_RESERVED 16

typedef struct {
    uintptr_t start;
    uintptr_t end; /* exclusive */
} phys_range_t;

static phys_range_t reserved[MAX_RESERVED];
static size_t reserved_count = 0;

#define PAGE_ALIGN_DOWN(x) ((x) & ~(uintptr_t)(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x) (((x) + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1))

#ifdef PAGING_DEBUG
#define DBG_MAX_PAGES (1ULL << 24) /* 64 GiB of physical memory */
static uint8_t dbg_alloc[DBG_MAX_PAGES / 8];

static inline int dbg_test(uintptr_t phys) {
    uint64_t pfn = phys >> 12;
    return pfn < DBG_MAX_PAGES && (dbg_alloc[pfn >> 3] & (1 << (pfn & 7)));
}
static inline void dbg_set(uintptr_t phys) {
    uint64_t pfn = phys >> 12;
    if (pfn < DBG_MAX_PAGES)
        dbg_alloc[pfn >> 3] |= (1 << (pfn & 7));
}
static inline void dbg_clear(uintptr_t phys) {
    uint64_t pfn = phys >> 12;
    if (pfn < DBG_MAX_PAGES)
        dbg_alloc[pfn >> 3] &= ~(1 << (pfn & 7));
}
#endif

static int overlaps_reserved(uintptr_t start, uintptr_t end) {
    for (size_t i = 0; i < reserved_count; i++) {
        if (start < reserved[i].end && end > reserved[i].start)
            return 1;
    }
    return 0;
}

/*
 * Mark [phys_start, phys_end) as never allocatable (kernel heap, framebuffer
 * backing, etc). Call this as early as possible, before the first
 * allocate_page(). Safe to call later: the current bump region is clipped.
 */
void paging_reserve_range(uintptr_t phys_start, uintptr_t phys_end) {
    phys_start = PAGE_ALIGN_DOWN(phys_start);
    phys_end = PAGE_ALIGN_UP(phys_end);

    if (phys_end <= phys_start)
        return;

    if (reserved_count >= MAX_RESERVED) {
        LOG_SCOPE();
        error("paging_reserve_range(): too many reserved ranges", __FILE__);
        return;
    }

    reserved[reserved_count].start = phys_start;
    reserved[reserved_count].end = phys_end;
    reserved_count++;

    /* If the active bump region already covers it, clip it. */
    if (bump_ptr < phys_end && bump_end > phys_start) {
        if (bump_ptr < phys_start) {
            bump_end = phys_start;
        } else {
            bump_ptr = bump_end = phys_end; /* forces a rescan from after the range */
        }
    }
}

/*
 * Finds the lowest usable physical range at or above `from` that can hold
 * `needed` bytes without touching a reserved range. On success *out_start is
 * where allocation begins and *out_end is the end of the safe run (clipped to
 * the next reserved range).
 */
static int find_free_region(uintptr_t from, size_t needed, uintptr_t *out_start, uintptr_t *out_end) {
    int found = 0;
    uintptr_t best_start = 0, best_end = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;

        uintptr_t region_start = PAGE_ALIGN_UP(e->base);
        uintptr_t region_end = PAGE_ALIGN_DOWN(e->base + e->length);
        if (region_end <= region_start)
            continue;

        uintptr_t p = PAGE_ALIGN_UP(region_start > from ? region_start : from);

        /* Slide forward past any reserved range we would overlap. */
        int moved;
        do {
            moved = 0;
            for (size_t r = 0; r < reserved_count; r++) {
                if (p < reserved[r].end && p + needed > reserved[r].start) {
                    p = reserved[r].end;
                    moved = 1;
                }
            }
        } while (moved);

        if (p + needed > region_end)
            continue;

        /* Clip the run so it stops before the next reserved range. */
        uintptr_t run_end = region_end;
        for (size_t r = 0; r < reserved_count; r++) {
            if (reserved[r].start >= p && reserved[r].start < run_end)
                run_end = reserved[r].start;
        }

        if (!found || p < best_start) {
            best_start = p;
            best_end = run_end;
            found = 1;
        }
    }

    if (!found)
        return 0;

    *out_start = best_start;
    *out_end = best_end;
    return 1;
}

/* ------------------------------------------------------------- helpers --- */

void paging_set_hhdm_offset(uint64_t offset) {
    hhdm_offset = offset;
}

static inline uint64_t *phys_to_virt_ptr(uint64_t phys_addr) {
    return (uint64_t *)(phys_addr + hhdm_offset);
}

void *paging_phys_to_virt(uintptr_t phys_addr) {
    return (void *)(phys_addr + hhdm_offset);
}

uint64_t fast_virt_to_phys(void *v) {
    return (uint64_t)v - hhdm_offset;
}

uint64_t virt_to_phys(void *v) {
    return fast_virt_to_phys(v);
}

static int phys_page_is_usable(uintptr_t phys);

/*
 * True if `phys` lies inside the kernel heap's physical backing. The heap
 * pages must never be freed, recycled, or mapped into user space. This is
 * checked directly against heap_begin/heap_end, so it works even if
 * paging_reserve_range() was never called for the heap.
 */
static inline int phys_in_heap(uintptr_t phys) {
    if (!heap_end || heap_end <= heap_begin)
        return 0;

    uintptr_t h0 = (uintptr_t)heap_begin - hhdm_offset;
    uintptr_t h1 = (uintptr_t)heap_end - hhdm_offset;
    return phys >= h0 && phys < h1;
}

/* ---------------------------------------------------------- allocation --- */

uintptr_t allocate_page(void) {
    if (!memmap) {
        LOG_SCOPE();
        error("Limine failed to give the memory map", __FILE__);
        hcf2();
    }

    // Prefer reclaiming a freed page over bump-allocating new memory.
    if (free_list_head) {
        uintptr_t page = free_list_head;

        if (phys_in_heap(page) || !phys_page_is_usable(page)) {
            /* The list is poisoned; abandon it rather than hand out a bad page. */
            error("allocate_page(): free list head %p is not recyclable, dropping list",
                __FILE__, (void *)page);
            free_list_head = 0;
        } else {
            uintptr_t next = *phys_to_virt_ptr(page); // next-pointer stashed by free_page()

            if (next && (phys_in_heap(next) || !phys_page_is_usable(next))) {
                /* Something wrote to this page after it was freed. */
                error("allocate_page(): page %p was written after free (next=%p), dropping list",
                    __FILE__, (void *)page, (void *)next);
                next = 0;
            }

            free_list_head = next;
            memset(phys_to_virt_ptr(page), 0, PAGE_SIZE);
#ifdef PAGING_DEBUG
            dbg_set(page);
#endif
            return page;
        }
    }

    if (!bump_ptr || bump_ptr + PAGE_SIZE > bump_end) {
        uintptr_t start, end;
        if (!find_free_region(bump_ptr, PAGE_SIZE, &start, &end)) {
            LOG_SCOPE();
            error("Out of physical memory", __FILE__);
            hcf2();
        }
        bump_ptr = start;
        bump_end = end;
    }

    uintptr_t page = bump_ptr;
    bump_ptr += PAGE_SIZE;
    memset(phys_to_virt_ptr(page), 0, PAGE_SIZE);
#ifdef PAGING_DEBUG
    dbg_set(page);
#endif
    return page;
}

/* Contiguous run of `count` pages. Never touches the free list. */
uintptr_t allocate_pages_contiguous(size_t count) {
    if (!memmap) {
        LOG_SCOPE();
        error("Limine failed to give the memory map", __FILE__);
        hcf2();
    }
    if (count == 0)
        return 0;

    size_t needed = count * PAGE_SIZE;
    uintptr_t base;

    if (bump_ptr && bump_ptr + needed <= bump_end) {
        base = bump_ptr;
        bump_ptr += needed;
    } else {
        uintptr_t start, end;
        if (!find_free_region(bump_ptr, needed, &start, &end)) {
            LOG_SCOPE();
            error("Out of contiguous physical memory", __FILE__);
            hcf2();
        }
        base = start;
        bump_ptr = start + needed;
        bump_end = end;
    }

    for (size_t i = 0; i < count; i++) {
        memset(phys_to_virt_ptr(base + i * PAGE_SIZE), 0, PAGE_SIZE);
#ifdef PAGING_DEBUG
        dbg_set(base + i * PAGE_SIZE);
#endif
    }
    return base;
}

/* Kept for compatibility: the result is always physically contiguous. */
uintptr_t allocate_pages(size_t count) {
    return allocate_pages_contiguous(count);
}

/* --------------------------------------------------------- page tables --- */

static inline uint64_t get_kernel_pml4(void) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

uint64_t paging_user_page_flags(uint64_t virt) {
    uint64_t *pml4 = phys_to_virt_ptr(get_kernel_pml4() & ~0xFFFULL);
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx = (virt >> 21) & 0x1FF;
    uint64_t pt_idx = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT))
        return 0;
    uint64_t *pdpt = phys_to_virt_ptr(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT))
        return 0;
    uint64_t *pd = phys_to_virt_ptr(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PAGE_PRESENT))
        return 0;
    uint64_t *pt = phys_to_virt_ptr(pd[pd_idx] & ~0xFFFULL);

    if (!(pt[pt_idx] & PAGE_PRESENT))
        return 0;

    return pt[pt_idx] & 0xFFF0000000000FFFULL;
}

void map_user_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (phys_in_heap(phys & PAGE_PHYS_ADDR_MASK)) {
        error("map_user_page(): mapping HEAP phys %p at virt %p, caller=%p",
            __FILE__, (void *)phys, (void *)virt, __builtin_return_address(0));
    }

    // Traverse or create PML4 -> PDPT -> PD -> PT
    uint64_t *pml4 = phys_to_virt_ptr(get_kernel_pml4() & ~0xFFFULL); // kernel PML4
    uint64_t *pdpt, *pd, *pt;
    uint64_t pdpt_phys, pd_phys, pt_phys;

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx = (virt >> 21) & 0x1FF;
    uint64_t pt_idx = (virt >> 12) & 0x1FF;

    // Create PDPT if missing
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        pdpt_phys = allocate_page();
        pdpt = phys_to_virt_ptr(pdpt_phys);
        memset(pdpt, 0, 0x1000);
        pml4[pml4_idx] = pdpt_phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        pml4[pml4_idx] |= (PAGE_RW | PAGE_USER);
        pdpt_phys = pml4[pml4_idx] & ~0xFFFULL;
        pdpt = phys_to_virt_ptr(pdpt_phys);
    }

    // Create PD if missing
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        pd_phys = allocate_page();
        pd = phys_to_virt_ptr(pd_phys);
        memset(pd, 0, 0x1000);
        pdpt[pdpt_idx] = pd_phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        pdpt[pdpt_idx] |= (PAGE_RW | PAGE_USER);
        pd_phys = pdpt[pdpt_idx] & ~0xFFFULL;
        pd = phys_to_virt_ptr(pd_phys);
    }

    // Create PT if missing
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        pt_phys = allocate_page();
        pt = phys_to_virt_ptr(pt_phys);
        memset(pt, 0, 0x1000);
        pd[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        pd[pd_idx] |= (PAGE_RW | PAGE_USER);
        pt_phys = pd[pd_idx] & ~0xFFFULL;
        pt = phys_to_virt_ptr(pt_phys);
    }

    pt[pt_idx] = phys | flags;

    // Flush TLB
    asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

bool paging_set_user_page_permissions(uint64_t virt, bool writable, bool executable) {
    uint64_t *pml4 = phys_to_virt_ptr(get_kernel_pml4() & ~0xFFFULL);
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx = (virt >> 21) & 0x1FF;
    uint64_t pt_idx = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT))
        return false;
    uint64_t *pdpt = phys_to_virt_ptr(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT))
        return false;
    uint64_t *pd = phys_to_virt_ptr(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & PAGE_PRESENT))
        return false;
    uint64_t *pt = phys_to_virt_ptr(pd[pd_idx] & ~0xFFFULL);
    if (!(pt[pt_idx] & PAGE_PRESENT))
        return false;

    if (writable)
        pt[pt_idx] |= PAGE_RW;
    else
        pt[pt_idx] &= ~((uint64_t)PAGE_RW);
    if (executable)
        pt[pt_idx] &= ~PAGE_NX;
    else
        pt[pt_idx] |= PAGE_NX;

    asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
    return true;
}

/* ------------------------------------------------------------- freeing --- */

static int phys_page_is_usable(uintptr_t phys) {
    if (!memmap)
        return 0;

    /* Physical pages must be page aligned. */
    if (phys & (PAGE_SIZE - 1))
        return 0;

    /* Reserved memory (kernel heap etc.) is never ours to recycle. */
    if (overlaps_reserved(phys, phys + PAGE_SIZE))
        return 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];

        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;

        uintptr_t region_start = e->base;
        uintptr_t region_end = e->base + e->length;

        /* Avoid overflow in base + length. */
        if (region_end < region_start)
            continue;

        if (phys >= region_start &&
            phys < region_end &&
            phys + PAGE_SIZE <= region_end)
            return 1;
    }

    return 0;
}

void free_page(uintptr_t phys) {
    /* Heap pages are never recyclable, reserved or not. */
    if (phys_in_heap(phys)) {
        LOG_SCOPE();
        error("free_page(): HEAP page %p freed, caller=%p",
            __FILE__, (void *)phys, __builtin_return_address(0));
        return;
    }

    /* Never dereference phys through the HHDM until it is proven valid. */
    if (!phys_page_is_usable(phys)) {
        LOG_SCOPE();
        error("free_page(): invalid or reserved physical page: %p, caller=%p",
            __FILE__, (void *)phys, __builtin_return_address(0));
        return;
    }

#ifdef PAGING_DEBUG
    if (!dbg_test(phys)) {
        LOG_SCOPE();
        error("free_page(): %p double free or never allocated, caller=%p",
            __FILE__, (void *)phys, __builtin_return_address(0));
        return; /* do NOT put it on the free list */
    }
    dbg_clear(phys);
#endif

    // Stash the next-pointer inside the freed page itself (via its HHDM mapping)
    uint64_t *v = phys_to_virt_ptr(phys);
    *v = free_list_head;
    free_list_head = phys;
}

static int table_is_empty(uint64_t *table) {
    for (int i = 0; i < 512; i++) {
        if (table[i] & PAGE_PRESENT)
            return 0;
    }
    return 1;
}

void unmap_user_page(uint64_t virt) {
    uint64_t *pml4 = phys_to_virt_ptr(get_kernel_pml4() & ~0xFFFULL);
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx = (virt >> 21) & 0x1FF;
    uint64_t pt_idx = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT))
        return;
    uint64_t pdpt_phys = pml4[pml4_idx] & ~0xFFFULL;
    uint64_t *pdpt = phys_to_virt_ptr(pdpt_phys);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT))
        return;
    uint64_t pd_phys = pdpt[pdpt_idx] & ~0xFFFULL;
    uint64_t *pd = phys_to_virt_ptr(pd_phys);

    if (!(pd[pd_idx] & PAGE_PRESENT))
        return;
    uint64_t pt_phys = pd[pd_idx] & ~0xFFFULL;
    uint64_t *pt = phys_to_virt_ptr(pt_phys);

    if (!(pt[pt_idx] & PAGE_PRESENT))
        return;

    uint64_t pte = pt[pt_idx]; /* capture before clearing */
    uint64_t data_phys = pte & PAGE_PHYS_ADDR_MASK;
    pt[pt_idx] = 0;
    asm volatile("invlpg (%0)" ::"r"(virt) : "memory");

    if (data_phys && phys_in_heap(data_phys)) {
        error("unmap_user_page(): virt %p maps HEAP phys %p (pte=%p)",
            __FILE__, (void *)virt, (void *)data_phys, (void *)pte);
        data_phys = 0; /* never recycle a heap page */
    }
    if (data_phys)
        free_page(data_phys);

    if (table_is_empty(pt)) {
        pd[pd_idx] = 0;
        free_page(pt_phys);
        if (table_is_empty(pd)) {
            pdpt[pdpt_idx] = 0;
            free_page(pd_phys);
            if (table_is_empty(pdpt)) {
                pml4[pml4_idx] = 0;
                free_page(pdpt_phys);
            }
        }
    }
}