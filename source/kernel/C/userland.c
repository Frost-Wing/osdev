/**
 * @file userland.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Main steps to enter Userland.
 * @version 0.1
 * @date 2025-10-03
 * 
 * @copyright Copyright (c) 2025
 * 
 */
 #include <userland.h>

void userland_main() {
    // This is now "userland"
    printf(">>> Entered userland!\n");

    // Call a syscall
    invoke_syscall(1);

    // Halt CPU after userland
    hcf2();
}

void enter_userland() {
    uint64_t user_stack_top = (uint64_t)&user_stack[USER_STACK_SIZE];

    asm volatile(
        "cli\n\t"                       // disable interrupts
        "movq %0, %%rsp\n\t"            // set stack pointer to user stack
        "pushq $0x23\n\t"               // user data segment selector (example)
        "pushq %0\n\t"                  // initial RSP for iretq
        "pushfq\n\t"                    // push RFLAGS
        "pushq $0x1B\n\t"               // user code segment selector
        "pushq $1f\n\t"                 // RIP
        "iretq\n\t"                      // switch to userland
        "1:\n\t"
        "call userland_main\n\t"        // jump to C userland function
        :
        : "r"(user_stack_top)
        : "memory"
    );
}
