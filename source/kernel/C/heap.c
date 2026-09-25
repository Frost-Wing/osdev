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

typedef struct alloc_t {
    uint64_t size;
    uint8_t status; // 0 = free, 1 = allocated
    uint64_t magic;
} __attribute__((packed)) alloc_t;

typedef struct {
    uint64_t magic;
    uintptr_t raw;
} aligned_alloc_header_t;

uint64_t heap_begin = 0;
uint64_t heap_end = 0;
uint64_t last_alloc = 0;
uint64_t alloc_count = 0;
uint64_t memory_used = 0;

static spinlock_t heap_lock = SPINLOCK_INITIALIZER;

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGNED_ALLOC_MAGIC 0x46574B414C49474EULL /* "FWKALIGN" */

static alloc_t *heap_alloc_from_user_ptr(void *ptr) {
    if (!ptr || heap_begin == 0 || heap_end <= heap_begin)
        return NULL;

    uintptr_t user = (uintptr_t)ptr;
    if (user < heap_begin + sizeof(alloc_t) || user > heap_end)
        return NULL;

    if (user >= heap_begin + sizeof(aligned_alloc_header_t)) {
        aligned_alloc_header_t *aligned_hdr =
            (aligned_alloc_header_t *)(user - sizeof(aligned_alloc_header_t));

        if ((uintptr_t)aligned_hdr >= heap_begin &&
            (uintptr_t)aligned_hdr <= heap_end - sizeof(*aligned_hdr) &&
            aligned_hdr->magic == ALIGNED_ALLOC_MAGIC &&
            aligned_hdr->raw >= heap_begin + sizeof(alloc_t) &&
            aligned_hdr->raw <= heap_end) {
            alloc_t *raw_alloc = (alloc_t *)(aligned_hdr->raw - sizeof(alloc_t));
            if ((uintptr_t)raw_alloc >= heap_begin &&
                (uintptr_t)raw_alloc <= heap_end - sizeof(*raw_alloc)) {
                return raw_alloc;
            }
        }
    }

    return (alloc_t *)(user - sizeof(alloc_t));
}

static bool kheap_check_locked(void) {
    if (heap_begin == 0 || heap_end <= heap_begin || last_alloc < heap_begin ||
        last_alloc > heap_end || (last_alloc & 7) != 0) {
        return false;
    }

    uint64_t checked_memory_used = 0;
    uint64_t checked_alloc_count = 0;
    uintptr_t cursor = heap_begin;

    while (cursor < last_alloc) {
        if (last_alloc - cursor < sizeof(alloc_t)) {
            return false;
        }

        alloc_t *allocation = (alloc_t *)cursor;
        if (allocation->status > 1 || allocation->size > last_alloc - cursor - sizeof(*allocation)) {
            return false;
        }

        uintptr_t next = ALIGN_UP(cursor + sizeof(*allocation) + allocation->size, 8);
        if (next <= cursor || next > last_alloc) {
            return false;
        }

        if (allocation->status) {
            checked_memory_used += allocation->size + sizeof(*allocation);
            checked_alloc_count++;
        }
        cursor = next;
    }

    return cursor == last_alloc && checked_memory_used == memory_used &&
           checked_alloc_count == alloc_count;
}

bool kheap_check(void) {
    spinlock_lock(&heap_lock);
    bool valid = kheap_check_locked();
    spinlock_unlock(&heap_lock);

    return valid;
}

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

    memset((void *)heap_begin, 0, (size_t)(heap_end - heap_begin));

    spinlock_unlock(&heap_lock);

    printf("heap begin -> 0x%X", heap_begin);
    printf("heap end   -> 0x%X", heap_end);

    done("Heap initialized", __FILE__);
}

static void *kmalloc_locked(size_t size) {

    uint8_t *mem = (uint8_t *)heap_begin;

    // Search for free block
    while ((uintptr_t)mem < last_alloc) {
        alloc_t *a = (alloc_t *)mem;

        if (a->size == 0)
            break;

        if (!a->status && a->size >= size) {
            a->status = 1;
            memory_used += a->size + sizeof(alloc_t);
            memset(mem + sizeof(alloc_t), 0, size);
            return mem + sizeof(alloc_t);
        }

        mem += sizeof(alloc_t) + a->size;
        mem = (uint8_t *)ALIGN_UP((uintptr_t)mem, 8);
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

    uint8_t *user_ptr = (uint8_t *)new_alloc + sizeof(alloc_t);
    memset(user_ptr, 0, size);

    last_alloc += sizeof(alloc_t) + size;
    last_alloc = ALIGN_UP(last_alloc, 8);

    memory_used += size + sizeof(alloc_t);

    alloc_count++;

    return user_ptr;
}

static bool kfree_locked(void *ptr) {
    alloc_t *a = heap_alloc_from_user_ptr(ptr);
    if (!a || (uintptr_t)a < heap_begin || (uintptr_t)a > heap_end - sizeof(*a)) {
        return false;
    }

    if (a->status == 0) {
        return false;
    }

    a->status = 0;
    memory_used -= a->size + sizeof(alloc_t);
    alloc_count--;
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

    return ptr;
}

void kfree(void *ptr) {
    if (!ptr) {
        LOG_SCOPE();
        warn("kfree: Cannot free null pointer", __FILE__);
        return;
    }

    spinlock_lock(&heap_lock);
    bool freed = kfree_locked(ptr);
    spinlock_unlock(&heap_lock);

    if (!freed) {
        warn("kfree: Invalid or already freed pointer.", __FILE__);
    }
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr)
        return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    size = ALIGN_UP(size, 8);

    spinlock_lock(&heap_lock);

    alloc_t *old = heap_alloc_from_user_ptr(ptr);
    if (!old || old->status == 0) {
        spinlock_unlock(&heap_lock);
        warn("krealloc: Invalid pointer", __FILE__);
        return NULL;
    }
    if (old->size >= size) {
        spinlock_unlock(&heap_lock);
        return ptr;
    }

    void *new_ptr = kmalloc_locked(size);

    memcpy(new_ptr, ptr, old->size);
    (void)kfree_locked(ptr);
    spinlock_unlock(&heap_lock);

    return new_ptr;
}

void *kmalloc_aligned(size_t size, size_t align) {
    if (align < sizeof(void *) || (align & (align - 1)) != 0) {
        warn("kmalloc_aligned: align must be a power of two", __FILE__);
        return NULL;
    }

    if (size == 0 || size > SIZE_MAX - align - sizeof(aligned_alloc_header_t)) {
        warn("kmalloc_aligned: invalid size", __FILE__);
        return NULL;
    }

    size_t total = size + align - 1 + sizeof(aligned_alloc_header_t);
    if (total > SIZE_MAX - 7) {
        warn("kmalloc_aligned: invalid size", __FILE__);
        return NULL;
    }
    total = ALIGN_UP(total, 8);
    spinlock_lock(&heap_lock);
    uintptr_t raw = (uintptr_t)kmalloc_locked(total);
    if (!raw) {
        spinlock_unlock(&heap_lock);
        return NULL;
    }

    uintptr_t aligned = ALIGN_UP(raw + sizeof(aligned_alloc_header_t), align);
    aligned_alloc_header_t *hdr =
        (aligned_alloc_header_t *)(aligned - sizeof(aligned_alloc_header_t));
    hdr->magic = ALIGNED_ALLOC_MAGIC;
    hdr->raw = raw;

    spinlock_unlock(&heap_lock);

    return (void *)aligned;
}

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