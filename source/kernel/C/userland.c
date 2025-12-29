#include <stdint.h>
#include <memory.h>
#include <basics.h>
#include <paging.h>

extern uint8_t user_code_start[];
extern uint8_t user_code_end[];

// Simple userland code
__attribute__((section(".user")))
__attribute__((naked))
void user_entry() {
    asm volatile (
        "1: pause\n"
        "jmp 1b\n"
    );
}

// Map user code and stack
// void setup_userland_heap() {
//     for (uint64_t offset = 0; offset < USER_HEAP_SIZE; offset += 0x1000) {
//         void *phys = allocate_page();  // get real physical page
//         map_user_page(USER_HEAP_VADDR + offset, (uint64_t)phys, 0); // non-executable
//     }
//     done("Userland heap mapped", __FILE__);
// }

void map_user_code_physical(uint64_t code_phys, uint64_t code_size) {
    uint64_t phys_page = code_phys & ~0xFFFULL;      // round down
    uint64_t offset = code_phys & 0xFFFULL;         // offset into first page
    uint64_t mapped = 0;

    while (mapped < code_size) {
        uint64_t to_map = PAGE_SIZE;
        map_user_page(USER_CODE_VADDR + mapped, phys_page, USER_CODE_FLAGS);

        mark_page_used(phys_page); // prevent reuse
        mapped += to_map - (mapped == 0 ? offset : 0);
        phys_page += PAGE_SIZE;
    }
}


void enter_userland(uint64_t code_phys, uint64_t code_size) {
    printf("Setting up kernel to move to userland...\n");

    uint64_t code_entry = code_phys;
    uint64_t stack_top  = USER_STACK_TOP;

    // Map the user code pages from the passed physical address
    map_user_code_physical(code_phys, code_size);

    // Map the user stack pages
    for (uint64_t off = 0; off < USER_STACK_SIZE; off += PAGE_SIZE) {
        uint64_t phys = (uint64_t)allocate_page();
        uint64_t vaddr = stack_top - off - PAGE_SIZE;
        map_user_page(vaddr, phys, USER_DATA_FLAGS);
    }

    // Switch to userland
    asm volatile (
        "cli\n"
        "pushq $0x23\n"        // User SS
        "pushq %0\n"           // User RSP
        "pushq $0x202\n"       // RFLAGS
        "pushq $0x1B\n"        // User CS
        "pushq %1\n"           // User RIP
        "iretq\n"
        :
        : "r"(stack_top), "r"(code_entry)
        : "memory"
    );
}
