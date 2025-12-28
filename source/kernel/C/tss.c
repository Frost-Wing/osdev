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
#include <memory.h>

// Global TSS and kernel stack
__attribute__((aligned(16)))
struct tss_entry tss;

__attribute__((aligned(16)))
uint8_t kernel_stack[0x4000]; // 16 KB kernel stack

// Initialize TSS
void tss_init(void) {
    memset(&tss, 0, sizeof(tss));

    tss.rsp0 = (uint64_t)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(struct tss_entry);

    struct tss_descriptor *desc = (struct tss_descriptor *)&gdt[5];

    uint64_t base  = (uint64_t)&tss;
    uint32_t limit = sizeof(struct tss_entry) - 1;

    desc->limit_low  = limit & 0xFFFF;
    desc->base_low   = base & 0xFFFF;
    desc->base_mid   = (base >> 16) & 0xFF;
    desc->access     = 0x89;              // Present | Type=9 (available TSS)
    desc->gran       = (limit >> 16) & 0x0F;
    desc->base_high  = (base >> 24) & 0xFF;
    desc->base_upper = (base >> 32);
    desc->reserved   = 0;

    done("TSS is ready, yet to be deployed", __FILE__);
}

// Load TSS using ltr instruction
void tss_load() {
    asm volatile("ltr %0" :: "r"((int16)0x28));
    done("Done loading TSS", __FILE__);
}
