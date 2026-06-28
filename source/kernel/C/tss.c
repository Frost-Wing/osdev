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

uint64_t kernel_stack_top = (uint64_t)&kernel_stack[0x4000];

// Initialize TSS
void kernel_tss_init(void) {
    memset(&tss, 0, sizeof(tss));

    tss.rsp0 = (uint64_t)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(struct tss_entry);

    struct tss_descriptor *desc = (struct tss_descriptor *)&gdt[5];

    uint64_t base  = (uint64_t)&tss;
    uint32_t limit = (uint32_t)(sizeof(struct tss_entry) - 1U);

    desc->limit_low  = (uint16_t)(limit & 0xFFFFU);
    desc->base_low   = (uint16_t)(base & 0xFFFFU);
    desc->base_mid   = (uint8_t)((base >> 16) & 0xFFU);
    desc->access     = 0x89;              // Present | Type=9 (available TSS)
    desc->gran       = (uint8_t)((limit >> 16) & 0x0FU);
    desc->base_high  = (uint8_t)((base >> 24) & 0xFFU);
    desc->base_upper = (uint32_t)(base >> 32);
    desc->reserved   = 0;

    done("TSS is ready, yet to be deployed", __FILE__);
}

// Load TSS using ltr instruction
void tss_load(void) {
    asm volatile("ltr %0" :: "r"((int16)0x28));
    done("Done loading TSS", __FILE__);
}
