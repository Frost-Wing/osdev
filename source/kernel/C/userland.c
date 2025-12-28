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
void setup_userland_heap() {
    for (uint64_t offset = 0; offset < USER_HEAP_SIZE; offset += 0x1000) {
        void *phys = allocate_page();  // get real physical page
        map_user_page(USER_HEAP_VADDR + offset, (uint64_t)phys, 0); // non-executable
    }
    done("Userland heap mapped", __FILE__);
}

void enter_userland() {
    printf("Setting up kernel to move to userland...\n");

    map_user_code();
    // setup_userland_heap();

    uint64_t stack_top  = USER_STACK_TOP;
    uint64_t code_entry = USER_CODE_VADDR;

    // Map user stack
    for (uint64_t off = 0; off < USER_STACK_SIZE; off += 0x1000) {
        void *phys = allocate_page();
        map_user_page(USER_STACK_TOP - off - 0x1000,
                      (uint64_t)phys,
                      USER_DATA_FLAGS);
    }

    done("Switching to userland...", __FILE__);
    debug_printf("User RIP target = %z\n", code_entry);
    debug_printf("User RSP target = %z\n", stack_top);

    // 🔥 Actually enter userland
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