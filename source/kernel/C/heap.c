/**
 * @file heap.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Kernel heap implementation, a bump allocator with first-fit reuse,
 *        block splitting, forward coalescing and header-corruption detection.
 * @version 0.2
 * @date 2026-09-25
 *
 * @copyright Copyright (c) Pradosh 2026
 *
 * Define HEAP_HALT_ON_CORRUPTION to meltdown + halt on the first corrupted
 * header instead of just reporting it.
 */

#include <debugger.h>
#include <graphics.h>
#include <heap.h>
#include <klog.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <meltdown.h>
#include <cc-asm.h>
#include <spinlock.h>
#include <paging.h>

typedef struct alloc_t {
    uint64_t size;
    uint8_t status; // 0 = free, 1 = allocated
    uint64_t magic;
} alloc_t;

_Static_assert(sizeof(alloc_t) == 24,
               "Unexpected allocation header size");

_Static_assert(_Alignof(alloc_t) >= 8,
               "Allocation header must be 8-byte aligned");

/* Sits directly below the pointer returned by kmalloc_aligned(). */
typedef struct {
    uint64_t magic;
    uintptr_t raw;   /* user pointer of the underlying kmalloc block */
    uint64_t align;  /* requested alignment, kept so krealloc can honour it */
} aligned_alloc_header_t;

_Static_assert(sizeof(aligned_alloc_header_t) == 24,
               "Unexpected aligned allocation header size");

uint64_t heap_begin = 0;
uint64_t heap_end = 0;
uint64_t last_alloc = 0;
uint64_t alloc_count = 0;
uint64_t memory_used = 0;

static spinlock_t heap_lock = SPINLOCK_INITIALIZER;

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGNED_ALLOC_MAGIC 0x46574B414C49474EULL /* "FWKALIGN" */

/* A free block is only split if the leftover can hold a header + this much. */
#define HEAP_MIN_SPLIT_PAYLOAD 16

/* ===================== Fault reporting ===================== */
/*
 * Faults are recorded while the heap lock is held (printing under the lock
 * could re-enter the allocator) and printed after it is released.
 */

typedef struct {
    const char *where;
    uintptr_t addr;
    uint64_t size;
    uint64_t magic;
    uint8_t status;
    uint64_t dump[4]; /* first 32 bytes at the bad header */
} heap_fault_t;

static heap_fault_t g_fault;
static volatile bool g_fault_pending = false;
static uintptr_t g_last_fault_addr = 0;

/* Call with heap_lock held. Remembers the first fault until it is flushed. */
static void heap_note_fault(const char *where, const alloc_t *a) {
    uintptr_t addr = (uintptr_t)a;

    if (g_fault_pending || addr == g_last_fault_addr)
        return; /* don't spam the same header on every allocation */

    g_fault.where = where;
    g_fault.addr = addr;
    g_fault.size = a ? a->size : 0;
    g_fault.magic = a ? a->magic : 0;
    g_fault.status = a ? a->status : 0;
    memset(g_fault.dump, 0, sizeof(g_fault.dump));
    if (a && addr >= heap_begin && addr + sizeof(g_fault.dump) <= heap_end)
        memcpy(g_fault.dump, a, sizeof(g_fault.dump));

    g_last_fault_addr = addr;
    g_fault_pending = true;
}

/* Call WITHOUT heap_lock held. */
static void heap_flush_fault(void) {
    if (!g_fault_pending)
        return;

    spinlock_lock(&heap_lock);
    heap_fault_t f = g_fault;
    bool had = g_fault_pending;
    g_fault_pending = false;
    spinlock_unlock(&heap_lock);

    if (!had)
        return;

    eprintf("heap: bad header (%s) at %x size=%x status=%x magic=%x",
        f.where, f.addr, f.size, f.status, f.magic);
    eprintf("heap: header bytes %x %x %x %x",
        f.dump[0], f.dump[1], f.dump[2], f.dump[3]);

#ifdef HEAP_HALT_ON_CORRUPTION
    meltdown_screen("Heap corruption detected!", __FILE__, __LINE__, 0, getCR2(), 0, null);
    hcf();
#endif
}

/* ===================== Header helpers ===================== */

/* Address one past the end of a block's payload. Always 8-aligned. */
static inline uintptr_t block_end(const alloc_t *a) {
    return (uintptr_t)a + sizeof(alloc_t) + a->size;
}

/*
 * Full structural validation of a block header. Pure (no side effects), safe
 * to call with heap_lock held. Checks position, alignment, canary, status and
 * that the block fits inside the used part of the heap.
 */
static bool heap_header_valid(const alloc_t *a) {
    uintptr_t addr = (uintptr_t)a;

    if (addr < heap_begin || addr >= last_alloc || (addr & 7) != 0 ||
        last_alloc - addr < sizeof(*a))
        return false;

    if (a->magic != HEAP_CANARY || a->status > 1)
        return false;

    if (a->size == 0 || (a->size & 7) != 0 ||
        a->size > last_alloc - addr - sizeof(*a))
        return false;

    return true;
}

/* Maps a user pointer (plain or from kmalloc_aligned) to its block header. */
static alloc_t *heap_resolve(void *ptr, aligned_alloc_header_t **out_ahdr) {
    if (out_ahdr)
        *out_ahdr = NULL;

    if (!ptr || heap_begin == 0 || last_alloc <= heap_begin)
        return NULL;

    uintptr_t user = (uintptr_t)ptr;
    if ((user & 7) != 0 || user < heap_begin + sizeof(alloc_t) || user >= last_alloc)
        return NULL;

    uintptr_t ah_addr = user - sizeof(aligned_alloc_header_t);
    if (ah_addr >= heap_begin + sizeof(alloc_t)) {
        aligned_alloc_header_t *ah = (aligned_alloc_header_t *)ah_addr;

        if (ah->magic == ALIGNED_ALLOC_MAGIC &&
            ah->raw >= heap_begin + sizeof(alloc_t) &&
            ah->raw <= ah_addr &&
            (ah->raw & 7) == 0) {
            if (out_ahdr)
                *out_ahdr = ah;
            return (alloc_t *)(ah->raw - sizeof(alloc_t));
        }
    }

    return (alloc_t *)(user - sizeof(alloc_t));
}

/*
 * Resolve + validate. Returns NULL for foreign pointers and for wiped
 * headers (already freed / merged); records a fault if the header looks
 * damaged. Call with heap_lock held.
 */
static alloc_t *heap_get_block(void *ptr, const char *where, aligned_alloc_header_t **out_ahdr) {
    alloc_t *a = heap_resolve(ptr, out_ahdr);
    if (!a)
        return NULL;

    uintptr_t addr = (uintptr_t)a;
    if (addr < heap_begin || addr >= last_alloc || (addr & 7) != 0 ||
        last_alloc - addr < sizeof(*a))
        return NULL;

    if (!heap_header_valid(a)) {
        if (a->magic != 0)
            heap_note_fault(where, a);
        return NULL;
    }

    if ((uintptr_t)ptr >= block_end(a))
        return NULL;

    return a;
}

/* ===================== Heap consistency check ===================== */

enum { HEAP_CHECK_OK = 0, HEAP_CHECK_STRUCT = 1, HEAP_CHECK_ACCOUNT = 2 };

static int kheap_check_locked(uint64_t *chk_used, uint64_t *chk_count) {
    if (heap_begin == 0 || heap_end <= heap_begin || last_alloc < heap_begin ||
        last_alloc > heap_end || (last_alloc & 7) != 0) {
        return HEAP_CHECK_STRUCT;
    }

    uint64_t checked_memory_used = 0;
    uint64_t checked_alloc_count = 0;
    uintptr_t cursor = heap_begin;

    while (cursor < last_alloc) {
        alloc_t *allocation = (alloc_t *)cursor;
        if (!heap_header_valid(allocation)) {
            heap_note_fault("kheap_check", allocation);
            return HEAP_CHECK_STRUCT;
        }

        uintptr_t next = block_end(allocation);
        if (next <= cursor || next > last_alloc) {
            heap_note_fault("kheap_check", allocation);
            return HEAP_CHECK_STRUCT;
        }

        if (allocation->status) {
            checked_memory_used += allocation->size + sizeof(*allocation);
            checked_alloc_count++;
        }
        cursor = next;
    }

    *chk_used = checked_memory_used;
    *chk_count = checked_alloc_count;

    if (cursor != last_alloc)
        return HEAP_CHECK_STRUCT;
    if (checked_memory_used != memory_used || checked_alloc_count != alloc_count)
        return HEAP_CHECK_ACCOUNT;
    return HEAP_CHECK_OK;
}

bool kheap_check(void) {
    uint64_t chk_used = 0, chk_count = 0;

    spinlock_lock(&heap_lock);
    int rc = kheap_check_locked(&chk_used, &chk_count);
    uint64_t used = memory_used, count = alloc_count;
    spinlock_unlock(&heap_lock);

    heap_flush_fault();

    if (rc == HEAP_CHECK_ACCOUNT) {
        eprintf("heap: accounting mismatch: walk used=%x count=%x, counters used=%x count=%x",
            chk_used, chk_count, used, count);
    }

    return rc == HEAP_CHECK_OK;
}

/* ===================== Init ===================== */

void mm_init(uintptr_t kernel_end, uint64 heap_size) {
    LOG_SCOPE();
    info("Initializing heap", __FILE__);

    spinlock_lock(&heap_lock);

    heap_begin = ALIGN_UP(kernel_end, 8);
    if (heap_size <= 0 || (uint64_t)heap_size > UINT64_MAX - heap_begin) {
        meltdown_screen("Invalid heap size!", __FILE__, __LINE__, 0, getCR2(), 0, null);
        hcf();
    }
    heap_end = heap_begin + (uint64_t)heap_size;
    last_alloc = heap_begin;
    alloc_count = 0;
    memory_used = 0;
    g_fault_pending = false;
    g_last_fault_addr = 0;

    memset((void *)heap_begin, 0, (size_t)(heap_end - heap_begin));

    spinlock_unlock(&heap_lock);

    printf("heap begin -> 0x%X", heap_begin);
    printf("heap end   -> 0x%X", heap_end);

    done("Heap initialized", __FILE__);
}

/* ===================== Allocation ===================== */

/* size must already be a multiple of 8 and non-zero. */
static void *kmalloc_locked(size_t size) {
    uintptr_t cursor = heap_begin;

    // First-fit search over existing blocks.
    while (cursor < last_alloc) {
        alloc_t *a = (alloc_t *)cursor;

        if (!heap_header_valid(a)) {
            /* Never walk past a damaged header; fall back to bump allocation. */
            heap_note_fault("kmalloc", a);
            break;
        }

        if (!a->status && a->size >= size) {
            /* Split off the tail if it is big enough to be a block itself. */
            size_t spare = a->size - size;
            if (spare >= sizeof(alloc_t) + HEAP_MIN_SPLIT_PAYLOAD) {
                alloc_t *rest = (alloc_t *)((uint8_t *)a + sizeof(alloc_t) + size);
                rest->size = spare - sizeof(alloc_t);
                rest->status = 0;
                rest->magic = HEAP_CANARY;
                a->size = size;
            }

            a->status = 1;
            memory_used += a->size + sizeof(alloc_t);
            alloc_count++;

            uint8_t *user = (uint8_t *)a + sizeof(alloc_t);
            memset(user, 0, size);
            return user;
        }

        cursor = block_end(a);
    }

    // Align new block start
    last_alloc = ALIGN_UP(last_alloc, 8);

    if (size > UINT64_MAX - last_alloc - sizeof(alloc_t) ||
        last_alloc + sizeof(alloc_t) + size > heap_end) {
        meltdown_screen("Heap out of memory!", __FILE__, __LINE__, 0, getCR2(), 0, null);
        hcf();
    }

    alloc_t *new_alloc = (alloc_t *)last_alloc;
    new_alloc->size = size;
    new_alloc->status = 1;
    new_alloc->magic = HEAP_CANARY;

    uint8_t *user_ptr = (uint8_t *)new_alloc + sizeof(alloc_t);
    memset(user_ptr, 0, size);

    last_alloc += sizeof(alloc_t) + size;
    last_alloc = ALIGN_UP(last_alloc, 8);

    memory_used += size + sizeof(alloc_t);
    alloc_count++;

    return user_ptr;
}

static bool kfree_locked(void *ptr) {
    aligned_alloc_header_t *ahdr = NULL;
    alloc_t *a = heap_get_block(ptr, "kfree", &ahdr);
    if (!a)
        return false;

    if (a->status == 0)
        return false; /* already free */

    if (ahdr)
        ahdr->magic = 0; /* a second free of the aligned pointer must not resolve */

    uintptr_t addr = (uintptr_t)a;

    memory_used -= a->size + sizeof(alloc_t);
    alloc_count--;
    a->status = 0;

    /* Forward coalesce: absorb every free block that directly follows. */
    uintptr_t next = block_end(a);
    while (next < last_alloc) {
        alloc_t *n = (alloc_t *)next;
        if (!heap_header_valid(n) || n->status != 0)
            break;

        a->size += sizeof(alloc_t) + n->size;
        memset(n, 0, sizeof(*n)); /* wipe the absorbed header */
        next = block_end(a);
    }

    /* If the block is now the tail of the heap, give it back to the bump area. */
    if (next >= last_alloc) {
        memset(a, 0, sizeof(*a));
        last_alloc = addr;
    }

    return true;
}

void *kmalloc(size_t size) {
    if (size == 0) {
        LOG_SCOPE();
        warn("kmalloc: Cannot allocate 0 bytes", __FILE__);
        klog_printf("[heap] kmalloc called with zero size");
        return NULL;
    }

    size = ALIGN_UP(size, 8);

    spinlock_lock(&heap_lock);
    void *ptr = kmalloc_locked(size);
    spinlock_unlock(&heap_lock);

    heap_flush_fault();
    return ptr;
}

void ikfree(void *ptr, const char *function, const char *file, int line) {
    if (!ptr) {
        LOG_SCOPE();
        warn("kfree: Cannot free null pointer", __FILE__);
        return;
    }

    spinlock_lock(&heap_lock);
    bool freed = kfree_locked(ptr);
    spinlock_unlock(&heap_lock);

    heap_flush_fault();

    if (!freed) {
        warn("kfree: Invalid or already freed pointer attempted to free in %s() %s:%d", __FILE__, function, file, line);
    }
}

/* ===================== Aligned allocation ===================== */

static bool aligned_total(size_t size, size_t align, size_t *total) {
    if (size == 0 || size > SIZE_MAX - align - sizeof(aligned_alloc_header_t) - 7)
        return false;

    *total = ALIGN_UP(size + align - 1 + sizeof(aligned_alloc_header_t), 8);
    return true;
}

static void *kmalloc_aligned_locked(size_t total, size_t align) {
    uintptr_t raw = (uintptr_t)kmalloc_locked(total);
    if (!raw)
        return NULL;

    uintptr_t aligned = ALIGN_UP(raw + sizeof(aligned_alloc_header_t), align);
    aligned_alloc_header_t *hdr =
        (aligned_alloc_header_t *)(aligned - sizeof(aligned_alloc_header_t));
    hdr->magic = ALIGNED_ALLOC_MAGIC;
    hdr->raw = raw;
    hdr->align = align;

    return (void *)aligned;
}

void *kmalloc_aligned(size_t size, size_t align) {
    if (align < sizeof(void *) || (align & (align - 1)) != 0) {
        warn("kmalloc_aligned: align must be a power of two", __FILE__);
        return NULL;
    }

    size_t total;
    if (!aligned_total(size, align, &total)) {
        warn("kmalloc_aligned: invalid size", __FILE__);
        return NULL;
    }

    spinlock_lock(&heap_lock);
    void *ptr = kmalloc_aligned_locked(total, align);
    spinlock_unlock(&heap_lock);

    heap_flush_fault();
    return ptr;
}

/* ===================== Realloc ===================== */

void *krealloc(void *ptr, size_t size) {
    if (!ptr)
        return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    size = ALIGN_UP(size, 8);

    spinlock_lock(&heap_lock);

    aligned_alloc_header_t *ahdr = NULL;
    alloc_t *old = heap_get_block(ptr, "krealloc", &ahdr);
    if (!old || old->status == 0) {
        spinlock_unlock(&heap_lock);
        heap_flush_fault();
        warn("krealloc: Invalid pointer", __FILE__);
        return NULL;
    }

    /* Bytes actually usable from ptr (less than old->size for aligned pointers). */
    size_t usable = (size_t)(block_end(old) - (uintptr_t)ptr);
    if (usable >= size) {
        spinlock_unlock(&heap_lock);
        return ptr;
    }

    void *new_ptr;
    if (ahdr) {
        /* Keep the original alignment guarantee. */
        size_t total;
        if (!aligned_total(size, (size_t)ahdr->align, &total)) {
            spinlock_unlock(&heap_lock);
            warn("krealloc: invalid size", __FILE__);
            return NULL;
        }
        new_ptr = kmalloc_aligned_locked(total, (size_t)ahdr->align);
    } else {
        new_ptr = kmalloc_locked(size);
    }

    if (!new_ptr) {
        spinlock_unlock(&heap_lock);
        return NULL;
    }

    memcpy(new_ptr, ptr, usable);
    (void)kfree_locked(ptr);
    spinlock_unlock(&heap_lock);

    heap_flush_fault();
    return new_ptr;
}

/* ===================== Stats ===================== */

void mm_print_out(void) {
    spinlock_lock(&heap_lock);
    uint64_t used = memory_used;
    uint64_t free = heap_end - last_alloc;
    uint64_t size = heap_end - heap_begin;
    spinlock_unlock(&heap_lock);

    LOG_SCOPE();
    info("%sMemory used :%s %u KiB", __FILE__, yellow_color, reset_color, used / (1 KiB));
    info("%sMemory free :%s %u KiB", __FILE__, yellow_color, reset_color, free / (1 KiB));
    info("%sHeap size   :%s %u KiB", __FILE__, yellow_color, reset_color, size / (1 KiB));
}