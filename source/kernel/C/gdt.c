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
#include <graphics.h>
#include <gdt.h>
#include <tss.h>

struct gdt_entry gdt[7];
struct gdt_ptr gdtp;

static void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low    = (uint16_t)(base & 0xFFFFU);
    gdt[i].base_middle = (uint8_t)((base >> 16) & 0xFFU);
    gdt[i].base_high   = (uint8_t)((base >> 24) & 0xFFU);
    gdt[i].limit_low   = (uint16)(limit & 0xFFFFU);
    gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0FU) | (gran & 0xF0U));
    gdt[i].access      = access;
}

void setup_gdt(void) {
    info("Setting up GDT...", __FILE__);

    /* Null descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);
    info("Null descriptor has been set", __FILE__);

    /* Kernel code: access=0x9A, gran: G=1,L=1 -> 0xA0 */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);
    info("Kernel code descriptor has been set", __FILE__);

    /* Kernel data: access=0x92, gran: G=1, L=0 -> 0x80 */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0x80);
    info("Kernel data descriptor has been set", __FILE__);

    /* User code: access=0xFA, gran: G=1,L=1 -> 0xA0 */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);
    info("User code descriptor has been set", __FILE__);

    /* User data: access=0xF2, gran: G=1,L=0 -> 0x80 */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0x80); // user data
    info("User data descriptor has been set", __FILE__);

    /* TSS Setup */
    kernel_tss_init();

    gdtp.limit = (uint16_t)(sizeof(gdt) - 1U);
    gdtp.base  = (uint64_t)&gdt;

    /* Load GDT, reload data segments, then reload CS with lretq. */
    asm volatile (
        "cli\n"
        "lgdt %0\n"
        "mov $0x10, %%ax\n"   // kernel data
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"       // kernel code
        "lea 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        : "m"(gdtp)
        : "rax", "memory"
    );

    done("GDT Successfully initialized!", __FILE__);

    tss_load();
}
