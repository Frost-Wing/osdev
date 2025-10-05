#include <stdint.h>
#include <memory.h>
#include <basics.h>
#include <paging.h>

#define USER_STACK_SIZE 0x4000      // 16 KB
#define USER_STACK_VADDR 0x70000000
#define USER_CODE_VADDR  0x40000000
#define USER_HEAP_VADDR 0x50000000
#define USER_HEAP_SIZE  0x100000   // 1 MB


__attribute__((aligned(16)))
uint8_t user_stack[USER_STACK_SIZE];

// Simple userland code
void userland_main() {
    info("HELLO FROM USERLAND!", __FILE__);
    for (;;) asm volatile("hlt");
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
    setup_userland_heap();

    uint64_t stack_top = USER_STACK_VADDR + USER_STACK_SIZE;
    uint64_t code_entry = USER_CODE_VADDR;

    done("Switching to userland...", __FILE__);

    info("userland_main @ virtual:", __FILE__);
    printf("  &userland_main = 0x%x", (uint64_t)&userland_main);

    info("user_code entry @ virtual:", __FILE__);
    printf("  user_code = 0x%x", code_entry);

    info("user stack top @ virtual:", __FILE__);
    printf("  user_stack_top = 0x%x", stack_top);

    info("Mapped pages:", __FILE__);
    printf("  USER_CODE_VADDR = 0x%x -> phys = 0x%x", USER_CODE_VADDR, (uint64_t)&userland_main);
    for (uint64_t offset = 0; offset < USER_STACK_SIZE; offset += 0x1000) {
        printf("  USER_STACK_VADDR+0x%x -> phys = 0x%x", offset, (uint64_t)&user_stack[offset]);
    }


    asm volatile(
        "cli\n\t"                  // disable interrupts
        "movq %0, %%rsp\n\t"       // load user stack
        "pushq $0x23\n\t"          // user data segment (DS)
        "pushq %0\n\t"             // RSP for user
        "pushq $0x202\n\t"         // RFLAGS with IF=1
        "pushq $0x1B\n\t"          // user code segment (CS)
        "pushq %1\n\t"             // RIP for userland
        "iretq\n\t"                // ring 3 jump
        :
        : "r"(stack_top), "r"(code_entry)
        : "memory"
    );

    warn("Should never reach here!", __FILE__);
}
