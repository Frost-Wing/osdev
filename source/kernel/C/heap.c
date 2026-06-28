#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include <graphics.h>
#include <debugger.h>
#include <heap.h>

typedef struct alloc_t {
    uint64_t size;
    uint8_t status; // 0 = free, 1 = allocated
} __attribute__((packed)) alloc_t;

typedef struct {
    uint64_t magic;
    uintptr_t raw;
} aligned_alloc_header_t;

uint64_t heap_begin = 0;
uint64_t heap_end   = 0;
uint64_t last_alloc = 0;
uint64_t alloc_count = 0;
uint64_t memory_used = 0;

#define ALIGN_UP(x, a) (((x) + ((a)-1)) & ~((a)-1))
#define ALIGNED_ALLOC_MAGIC 0x46574B414C49474EULL /* "FWKALIGN" */

static alloc_t* heap_alloc_from_user_ptr(void* ptr)
{
    if (!ptr || heap_begin == 0 || heap_end <= heap_begin)
        return NULL;

    uintptr_t user = (uintptr_t)ptr;
    if (user < heap_begin + sizeof(alloc_t) || user > heap_end)
        return NULL;

    if (user >= heap_begin + sizeof(aligned_alloc_header_t)) {
        aligned_alloc_header_t* aligned_hdr =
            (aligned_alloc_header_t*)(user - sizeof(aligned_alloc_header_t));

        if ((uintptr_t)aligned_hdr >= heap_begin &&
            (uintptr_t)aligned_hdr <= heap_end - sizeof(*aligned_hdr) &&
            aligned_hdr->magic == ALIGNED_ALLOC_MAGIC &&
            aligned_hdr->raw >= heap_begin + sizeof(alloc_t) &&
            aligned_hdr->raw <= heap_end) {
            alloc_t* raw_alloc = (alloc_t*)(aligned_hdr->raw - sizeof(alloc_t));
            if ((uintptr_t)raw_alloc >= heap_begin &&
                (uintptr_t)raw_alloc <= heap_end - sizeof(*raw_alloc)) {
                return raw_alloc;
            }
        }
    }

    return (alloc_t*)(user - sizeof(alloc_t));
}

void mm_init(uintptr_t kernel_end, int64 heap_size)
{
    info("Initializing heap", __FILE__);

    heap_begin = ALIGN_UP(kernel_end, 8);
    if (heap_size <= 0 || (uint64_t)heap_size > UINT64_MAX - heap_begin) {
        meltdown_screen("Invalid heap size!", __FILE__, __LINE__, 0, getCR2(), 0);
        hcf();
    }
    heap_end   = heap_begin + (uint64_t)heap_size;
    last_alloc = heap_begin;

    memset((void*)heap_begin, 0, (size_t)(heap_end - heap_begin));

    printf("heap begin -> 0x%X", heap_begin);
    printf("heap end   -> 0x%X", heap_end);

    done("Heap initialized", __FILE__);
}

void* kmalloc(size_t size)
{
    if (size == 0) {
        warn("kmalloc: Cannot allocate 0 bytes", __FILE__);
        return NULL;
    }

    size = ALIGN_UP(size, 8);

    uint8_t* mem = (uint8_t*)heap_begin;

    // Search for free block
    while ((uintptr_t)mem < last_alloc) {
        alloc_t* a = (alloc_t*)mem;

        if (a->size == 0)
            break;

        if (!a->status && a->size >= size) {
            a->status = 1;
            memory_used += a->size + sizeof(alloc_t);
            memset(mem + sizeof(alloc_t), 0, size);
            return mem + sizeof(alloc_t);
        }

        mem += sizeof(alloc_t) + a->size;
        mem = (uint8_t*)ALIGN_UP((uintptr_t)mem, 8);
    }

    // Align new block start
    last_alloc = ALIGN_UP(last_alloc, 8);

    if (size > UINT64_MAX - last_alloc - sizeof(alloc_t) ||
        last_alloc + sizeof(alloc_t) + size > heap_end) {
        meltdown_screen("Heap out of memory!", __FILE__, __LINE__, 0, getCR2(), 0);
        hcf();
    }

    alloc_t* new_alloc = (alloc_t*)last_alloc;
    new_alloc->size = size;
    new_alloc->status = 1;

    uint8_t* user_ptr = (uint8_t*)new_alloc + sizeof(alloc_t);
    memset(user_ptr, 0, size);

    last_alloc += sizeof(alloc_t) + size;
    last_alloc = ALIGN_UP(last_alloc, 8);

    memory_used += size + sizeof(alloc_t);

    alloc_count++;

    return user_ptr;
}

void kfree(void* ptr)
{
    if (!ptr) {
        warn("kfree: Cannot free null pointer", __FILE__);
        return;
    }

    alloc_t* a = heap_alloc_from_user_ptr(ptr);
    if (!a || (uintptr_t)a < heap_begin || (uintptr_t)a > heap_end - sizeof(*a)) {
        warn("kfree: Pointer is outside heap.", __FILE__);
        return;
    }

    if (a->status == 0) {
        warn("kfree: Double free detected.", __FILE__);
        return;
    }

    a->status = 0;
    memory_used -= a->size + sizeof(alloc_t);
    alloc_count--;
}

void* krealloc(void* ptr, size_t size)
{
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    size = ALIGN_UP(size, 8);

    alloc_t* old = heap_alloc_from_user_ptr(ptr);
    if (!old || old->status == 0) {
        warn("krealloc: Invalid pointer", __FILE__);
        return NULL;
    }
    if (old->size >= size) return ptr;

    void* new_ptr = kmalloc(size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, old->size);
    kfree(ptr);
    return new_ptr;
}

void* kmalloc_aligned(size_t size, size_t align)
{
    if (align < sizeof(void*) || (align & (align - 1)) != 0) {
        warn("kmalloc_aligned: align must be a power of two", __FILE__);
        return NULL;
    }

    if (size == 0 || size > SIZE_MAX - align - sizeof(aligned_alloc_header_t)) {
        warn("kmalloc_aligned: invalid size", __FILE__);
        return NULL;
    }

    size_t total = size + align - 1 + sizeof(aligned_alloc_header_t);
    uintptr_t raw = (uintptr_t)kmalloc(total);
    if (!raw) {
        return NULL;
    }

    uintptr_t aligned = ALIGN_UP(raw + sizeof(aligned_alloc_header_t), align);
    aligned_alloc_header_t* hdr =
        (aligned_alloc_header_t*)(aligned - sizeof(aligned_alloc_header_t));
    hdr->magic = ALIGNED_ALLOC_MAGIC;
    hdr->raw = raw;

    return (void*)aligned;
}

void mm_print_out()
{
    printf("%sMemory used :%s %u KiB", yellow_color, reset_color, memory_used/(1 KiB));
    printf("%sMemory free :%s %u KiB", yellow_color, reset_color, (heap_end - last_alloc)/(1 KiB));
    printf("%sHeap size   :%s %u KiB", yellow_color, reset_color, (heap_end - heap_begin)/(1 KiB));
    debug_printf("%sMemory used :%s %u KiB\n", yellow_color, reset_color, memory_used/(1 KiB));
    debug_printf("%sMemory free :%s %u KiB\n", yellow_color, reset_color, (heap_end - last_alloc)/(1 KiB));
    debug_printf("%sHeap size   :%s %u KiB\n", yellow_color, reset_color, (heap_end - heap_begin)/(1 KiB));
}
