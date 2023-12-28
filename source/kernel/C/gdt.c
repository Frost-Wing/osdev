/**
 * @file gdt.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Global Descriptor Table for FrostWing
 * @version 0.1
 * @date 2023-10-23
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <gdt.h>

// Function to load the GDT
void load_gdt(struct gdt_pointer* gdt_ptr) {
    #if defined (__x86_64__)
        asm volatile(
            "lgdt (%0)"
            :
            : "r"(gdt_ptr)
        );
    #elif defined (__aarch64__)
        asm volatile(
            "msr ttbr0_el1, %0" // AARCH64-specific assembly code
            :
            : "r"(gdt_ptr)
        );
    #elif defined (__arm__) || defined (__aarch32__)
        asm volatile(
            "mcr p15, 0, %0, c2, c0, 0" // AARCH32-specific assembly code
            :
            : "r"(gdt_ptr)
        );
    #elif defined(__riscv)
        asm volatile(
            "lw t0, 0(%0)\n"
            "csrw sstatus, t0"
            :
            : "r"(gdt_ptr)
            : "t0"
        );
    #endif
}

// Global Descriptor Table (GDT) entries
struct gdt_entry gdt[3];

/**
 * @brief The Main GDT Initializing function.
 * 
 */
void gdt_init() {
    // Null descriptor
    gdt[0].limit_low = 0;
    gdt[0].base_low = 0;
    gdt[0].base_middle = 0;
    gdt[0].access = 0;
    gdt[0].granularity = 0;
    gdt[0].base_high = 0;

    info("Successfully setup-ed Null Descriptor!", __FILE__);

    // Code descriptor (64-bit code segment)
    gdt[1].limit_low = 0xFFFF;
    gdt[1].base_low = 0;
    gdt[1].base_middle = 0;
    gdt[1].access = 0x9A;  // Present, Ring 0, Code, Read/Execute
    gdt[1].granularity = 0xCF;  // 4KB granularity, 64-bit mode
    gdt[1].base_high = 0;

    info("Successfully setup-ed Code Descriptor! (64-bit)", __FILE__);

    // Data descriptor (64-bit data segment)
    gdt[2].limit_low = 0xFFFF;
    gdt[2].base_low = 0;
    gdt[2].base_middle = 0;
    gdt[2].access = 0x92;  // Present, Ring 0, Data, Read/Write
    gdt[2].granularity = 0xCF;  // 4KB granularity, 64-bit mode
    gdt[2].base_high = 0;

    info("Successfully setup-ed Data Descriptor! (64 bit)", __FILE__);

    // Set up GDT pointer
    struct gdt_pointer gdtp;
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (int64)&gdt;

    info("Setting up pointers....", __FILE__);

    // Load the GDT
    load_gdt(&gdtp);

    done("Successfully loaded GDT!", __FILE__);
}