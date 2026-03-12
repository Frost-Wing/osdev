#include <stdint.h>
#include <memory.h>
#include <basics.h>
#include <paging.h>

static void map_user_stack(void) {
    uint64_t stack_top = USER_STACK_TOP;

    for (uint64_t off = 0; off < USER_STACK_SIZE; off += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        uint64_t vaddr = stack_top - off - PAGE_SIZE;
        map_user_page(vaddr, phys, USER_DATA_FLAGS);
    }
}

/**
 * @brief Enter userland (ring 3) at a specific userspace RIP.
 */
void enter_userland_at(uint64_t code_entry) {
    uint64_t stack_top = USER_STACK_TOP;

    map_user_stack();

    printf("Switching to userland at 0x%x with stack 0x%x", code_entry, stack_top);

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
