#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include <graphics.h>
#include <heap.h>

typedef struct alloc_t {
    uint64_t size;
    uint8_t status; // 0 = free, 1 = allocated
} __attribute__((packed)) alloc_t;

uint64_t heap_begin = 0;
uint64_t heap_end   = 0;
uint64_t last_alloc = 0;
uint64_t memory_used = 0;

#define ALIGN_UP(x, a) (((x) + ((a)-1)) & ~((a)-1))

void mm_init(uintptr_t kernel_end)
{
    info("Initializing heap.", __FILE__);

    heap_begin = ALIGN_UP(kernel_end + 8 KiB, 8);
    heap_end   = heap_begin + 64 MiB;
    last_alloc = heap_begin;

    memset((void*)heap_begin, 0, 64 MiB);

    printf("heap begin -> 0x%X\nheap end -> 0x%X", heap_begin, heap_end);

    done("Heap initialized.", __FILE__);
}

void* kmalloc(size_t size)
{
    if (size == 0) {
        warn("kmalloc: Cannot allocate 0 bytes.", __FILE__);
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

    if (last_alloc + sizeof(alloc_t) + size > heap_end) {
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

    return user_ptr;
}

void kfree(void* ptr)
{
    if (!ptr) {
        warn("kfree: Cannot free null pointer.", __FILE__);
        return;
    }

    alloc_t* a = (alloc_t*)((uint8_t*)ptr - sizeof(alloc_t));

    if (a->status == 0) {
        warn("kfree: Double free detected.", __FILE__);
        return;
    }

    a->status = 0;
    memory_used -= a->size + sizeof(alloc_t);
}

void* krealloc(void* ptr, size_t size)
{
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    alloc_t* old = (alloc_t*)((uint8_t*)ptr - sizeof(alloc_t));
    if (old->size >= size) return ptr;

    void* new_ptr = kmalloc(size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, old->size);
    kfree(ptr);
    return new_ptr;
}

void mm_print_out()
{
    printf("%sMemory used :%s %u KiB", yellow_color, reset_color, memory_used/(1 KiB));
    printf("%sMemory free :%s %u KiB", yellow_color, reset_color, (heap_end - last_alloc)/(1 KiB));
    printf("%sHeap size   :%s %u KiB", yellow_color, reset_color, (heap_end - heap_begin)/(1 KiB));
}
