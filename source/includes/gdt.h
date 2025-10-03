/**
 * @file gdt.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-10-03
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef GDT_H
#define GDT_H
#include <basics.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern struct gdt_entry gdt[7];
extern struct gdt_ptr gdtp;

void setup_gdt();
#endif