/**
 * @file gdt.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Global Descriptor Table for the OS.
 * @version 0.1
 * @date 2025-10-03
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */
#include <gdt.h>

struct gdt_entry gdt[7];
struct gdt_ptr gdtp;

static void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_middle = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

void setup_gdt() {
    info("Setting up GDT...", __FILE__);

    /* Null descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Kernel code: access=0x9A, gran: G=1,L=1 -> 0xA0 */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    /* Kernel data: access=0x92, gran: G=1, L=0 -> 0x80 */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0x80);

    /* User code: access=0xFA, gran: G=1,L=1 -> 0xA0 */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);

    /* User data: access=0xF2, gran: G=1,L=0 -> 0x80 */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0x80);

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint64_t)&gdt;

    /* Load GDT, reload data segments, then reload CS with lretq. */
    asm volatile (
        "cli\n\t"                 /* disable interrupts during transition */
        "lgdt %0\n\t"             /* load new GDT */
        /* reload data segment registers with kernel data selector (index 2 -> 0x10) */
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        /* lretq to reload CS (kernel code selector = index 1 -> 0x08) */
        "pushq $0x08\n\t"         /* kernel code selector */
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        : : "m"(gdtp) : "rax", "memory"
    );

    done("GDT Successfully initialized!", __FILE__);
}
