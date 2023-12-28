/**
 * @file gdt.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Headers for GDT
 * @version 0.1
 * @date 2023-12-17
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>

#define kernel_stack_size 4096

struct gdt_entry {
    int16 limit_low;
    int16 base_low;
    int8 base_middle;
    int8 access;
    int8 granularity;
    int8 base_high;
};

struct gdt_pointer {
    int16 limit;
    int64 base;
};

struct tss_entry {
    int32 reserved;
    int64 rsp[3];  // Stack pointers for privilege levels 0, 1, and 2
    int64 reserved2;
    int64 ist[7];  // Interrupt Stack Table
    int64 reserved3;
    int16 reserved4;
    int16 io_map_base;
} attribute((packed)) attribute((aligned(16)));