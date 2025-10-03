/**
 * @file gdt.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Header file for Global Descriptor Table.
 * @version 0.1
 * @date 2025-10-03
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */
#ifndef GDT_H
#define GDT_H
#include <basics.h>

/**
 * @brief The GDT Table.
 * 
 */
struct gdt_entry {
    int16 limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/**
 * @brief GDT Pointer.
 * 
 */
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern struct gdt_entry gdt[7];
extern struct gdt_ptr gdtp;

/**
 * @brief Set the up GDT for the entire OS.
 * 
 */
void setup_gdt();
#endif