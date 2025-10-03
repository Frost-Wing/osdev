/**
 * @file userland.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Main steps to enter Userland.
 * @version 0.1
 * @date 2025-10-03
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */
#include <userland.h>

uint8_t user_stack[USER_STACK_SIZE] __attribute__((aligned(16)));

void userland_main() {
    asm volatile (
        "movq %0, %%rax\n\t"
        "int $0x80\n\t"
        :
        : "g" ((int64)1)
        : "rax"
    );

    for(;;) asm volatile("hlt");
}

void enter_userland() {
    setup_userland_memory();
    done("Setuped memory for userland", __FILE__);
    
    uint64_t user_stack_top = 0x70000000 + USER_STACK_SIZE; // mapped stack top
    uint64_t user_code = 0x40000000; // mapped code entry

    done("Expect to jump to userland_main()", __FILE__);

    asm volatile(
        "cli\n\t"
        "movq %0, %%rsp\n\t"
        "pushq $0x23\n\t"        // user data segment
        "pushq %0\n\t"           // user RSP
        "pushq $0x202\n\t"       // RFLAGS, IF=1
        "pushq $0x1B\n\t"        // user code segment
        "pushq %1\n\t"           // user RIP
        "iretq\n\t"
        :
        : "r"(user_stack_top), "r"(user_code)
        : "memory"
    );

    warn("This should not execute something went wrong", __FILE__);
}
