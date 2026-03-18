#include <stdint.h>
#include <memory.h>
#include <basics.h>
#include <paging.h>
#include <userland.h>

static uint64_t user_heap_break = USER_HEAP_VADDR;
static uint64_t user_heap_mapped_end = USER_HEAP_VADDR;

static uint64_t user_mmap_cursor = USER_MMAP_VADDR;
static uint64_t user_mmap_end = USER_MMAP_VADDR;

static void map_user_stack(void) {
    uint64_t stack_top = USER_STACK_TOP;

    for (uint64_t off = 0; off < USER_STACK_SIZE; off += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        uint64_t vaddr = stack_top - off - PAGE_SIZE;
        map_user_page(vaddr, phys, USER_DATA_FLAGS);
    }
}

static void map_user_range(uint64_t start, uint64_t end, uint64_t flags) {
    uint64_t aligned_start = start & ~(PAGE_SIZE - 1);
    uint64_t aligned_end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint64_t vaddr = aligned_start; vaddr < aligned_end; vaddr += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        map_user_page(vaddr, phys, flags);
    }
}

void userland_heap_init(void) {
    user_heap_break = USER_HEAP_VADDR;
    user_heap_mapped_end = USER_HEAP_VADDR;

    user_mmap_cursor = USER_MMAP_VADDR;
    user_mmap_end = USER_MMAP_VADDR + USER_MMAP_SIZE;
}

uint64_t userland_brk(uint64_t requested_break) {
    uint64_t heap_end = USER_HEAP_VADDR + USER_HEAP_SIZE;

    if (requested_break == 0) {
        return user_heap_break;
    }

    if (requested_break < USER_HEAP_VADDR || requested_break > heap_end) {
        return user_heap_break;
    }

    if (requested_break > user_heap_mapped_end) {
        map_user_range(user_heap_mapped_end, requested_break, USER_DATA_FLAGS);
        user_heap_mapped_end = (requested_break + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }

    user_heap_break = requested_break;
    return user_heap_break;
}

uint64_t userland_mmap_anon(uint64_t length) {
    if (length == 0) {
        return 0;
    }

    uint64_t aligned_len = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (user_mmap_cursor + aligned_len > user_mmap_end) {
        return 0;
    }

    uint64_t mapping_base = user_mmap_cursor;
    map_user_range(mapping_base, mapping_base + aligned_len, USER_DATA_FLAGS);
    user_mmap_cursor += aligned_len;

    return mapping_base;
}

/**
 * @brief Enter userland (ring 3) at a specific userspace RIP.
 */
void enter_userland_at(uint64_t code_entry) {
    uint64_t stack_top = USER_STACK_TOP;

    map_user_stack();
    userland_heap_init();

    // printf("Switching to userland at 0x%x with stack 0x%x", code_entry, stack_top);

    asm volatile (
        "cli\n"
        "pushq $0x23\n"        // User SS
        "pushq %0\n"           // User RSP
        "pushq $0x202\n"       // RFLAGS (IF = 1)
        "pushq $0x1B\n"        // User CS
        "pushq %1\n"           // User RIP
        "iretq\n"
        :
        : "r"(stack_top), "r"(code_entry)
        : "memory"
    );
}
