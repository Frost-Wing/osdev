/**
 * @file tss.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The TSS Header file.
 * @version 0.1
 * @date 2025-10-03
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */
#include <tss.h>
#include <gdt.h>
#include <memory2.h>

// Global TSS and kernel stack
__attribute__((aligned(16)))
struct tss_entry tss;

__attribute__((aligned(16)))
uint8_t kernel_stack[0x4000]; // 16 KB kernel stack

// Initialize TSS
void tss_init() {
    memset(&tss, 0, sizeof(tss));

    // RSP0 = kernel stack pointer used on ring 0 transitions
    tss.rsp0 = (uint64_t)&kernel_stack[sizeof(kernel_stack)];

    tss.iomap_base = sizeof(tss);

    // Add TSS descriptor to GDT (x86_64 TSS uses 2 entries)
    // Assuming GDT index 5 (selector = 0x28)
    uint64_t base = (uint64_t)&tss;
    uint32_t limit = sizeof(tss) - 1;

    // Lower 8 bytes of TSS descriptor
    gdt[5].limit_low    = limit & 0xFFFF;
    gdt[5].base_low     = base & 0xFFFF;
    gdt[5].base_middle  = (base >> 16) & 0xFF;
    gdt[5].access       = 0x89; // present=1, type=9 (available 64-bit TSS)
    gdt[5].granularity  = ((limit >> 16) & 0x0F);
    gdt[5].base_high    = (base >> 24) & 0xFF;

    // Upper 8 bytes of TSS descriptor
    uint64_t *upper = (uint64_t*)&gdt[6];
    *upper = (base >> 32) & 0xFFFFFFFFULL;

    // Note: index 6 in GDT is occupied by upper TSS descriptor
}

// Load TSS using ltr instruction
void tss_load() {
    asm volatile("ltr %%ax" :: "a"(0x28));
}
