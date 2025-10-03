/**
 * @file tss.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-10-03
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef TSS_H
#define TSS_H

#include <basics.h>   // uint64_t, uint8_t, etc.
#include <gdt.h>      // gdt_set_entry

// x86_64 TSS structure (16-byte aligned)
struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

extern struct tss_entry tss;
extern uint8_t kernel_stack[0x4000]; // 16 KB kernel stack

void tss_init();
void tss_load();

#endif
