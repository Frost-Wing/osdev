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
        "1: hlt\n"
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
    map_user_code();
    setup_userland_heap();

    uint64_t stack_top = USER_STACK_VADDR + USER_STACK_SIZE;
    uint64_t code_entry = USER_CODE_VADDR;

    // Map user stack pages downward
    for (uint64_t off = 0; off < USER_STACK_SIZE; off += 0x1000) {
        void *phys = allocate_page();
        map_user_page(stack_top - off - 0x1000, (uint64_t)phys, 0); // RW, non-executable
    }
        
    done("Switching to userland...", __FILE__);

    asm volatile (
        "cli\n"
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushq $0x23\n"
        "pushq %0\n"
        "pushfq\n"
        "pushq $0x1B\n"
        "pushq %1\n"
        "iretq\n"
        :
        : "r"(stack_top), "r"(code_entry)
        : "memory"
    );
}

