/**
 * @file tss.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Header file for TSS.
 * @version 0.1
 * @date 2025-10-03
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */
#ifndef TSS_H
#define TSS_H

#include <basics.h>   // uint64_t, uint8_t, etc.
#include <graphics.h>
#include <gdt.h>      // gdt_set_entry

/**
 * @brief x86_64 TSS Structure
 * 
 */
struct __attribute__((packed)) tss_entry {
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
};

struct __attribute__((packed)) tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
};

extern struct tss_entry tss;
/**
 * @brief 16 KB kernel stack
 * 
 */
extern uint8_t kernel_stack[0x4000];

/**
 * @brief Initialize TSS.
 * 
 */
void kernel_tss_init();

/**
 * @brief Load the TSS.
 * 
 */
void tss_load();

#endif
