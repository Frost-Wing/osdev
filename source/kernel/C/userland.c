#include <stdint.h>
#include <memory.h>
#include <basics.h>
#include <paging.h>

extern uint8_t user_code_start[];
extern uint8_t user_code_end[];

/**
 * @brief Map the user code into virtual memory.
 *
 * @param code_phys Physical address of the user code
 * @param code_size Size in bytes
 */
void map_user_code_physical(uint64_t code_phys, uint64_t code_size) {
    uint64_t phys_page = code_phys & ~0xFFFULL;
    uint64_t page_offset = code_phys & 0xFFFULL;
    uint64_t mapped = 0;

    while (mapped < code_size) {
        map_user_page(USER_CODE_VADDR + mapped, phys_page, USER_CODE_FLAGS & ~PAGE_NX);

        if (mapped == 0 && page_offset != 0)
            mapped += PAGE_SIZE - page_offset;
        else
            mapped += PAGE_SIZE;

        phys_page += PAGE_SIZE;
    }
}

/**
 * @brief Enter userland (ring 3) safely
 */
void enter_userland() {
    printf("Setting up kernel to move to userland...");

    // Compute physical addresses of user code
    uint64_t code_phys = virtual_to_physical((uint64_t)user_code_start);
    uint64_t code_size = (uint64_t)user_code_end - (uint64_t)user_code_start;
    uint64_t stack_top  = USER_STACK_TOP;
    uint64_t code_entry = USER_CODE_VADDR; // virtual address where user code will run

    // Map user code
    map_user_code_physical(code_phys, code_size);

    // Map the user stack
    for (uint64_t off = 0; off < USER_STACK_SIZE; off += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        uint64_t vaddr = stack_top - off - PAGE_SIZE;
        map_user_page(vaddr, phys, USER_DATA_FLAGS);
    }

    // Optional: Map user heap (commented for now)
    /*
    for (uint64_t off = 0; off < USER_HEAP_SIZE; off += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        map_user_page(USER_HEAP_VADDR + off, phys, USER_DATA_FLAGS);
    }
    */

    printf("Switching to userland at 0x%x with stack 0x%x", code_entry, stack_top);

    // Jump to userland (ring 3) using iretq
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

/**
 * @brief Simple user code (halt in a loop)
 */
__attribute__((section(".user")))
__attribute__((naked))
void user_entry() {
    asm volatile (
        "1: pause\n"   // pause CPU in an infinite loop
        "jmp 1b\n"
    );
}