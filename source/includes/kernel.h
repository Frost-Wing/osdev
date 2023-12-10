/**
 * @file kernel.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The basic requirement from kernel to other parts of the code.
 * @version 0.1
 * @date 2023-10-23
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <flanterm/flanterm.h>
#include <fb.h>
#include <hal.h>
#include <acpi.h>
#include <acpi-shutdown.h>
#include <pci.h>
#include <graphics.h>
#include <opengl/glcontext.h>
#include <opengl/glbackend.h>
#include <stddef.h>
#include <stdint.h>
#include <limine.h>
#include <memory.h>
#include <strings.h>
#include <math/fpu.h>
#include <sse.h>
#include <debugger.h>
#include <drivers/serial.h>
#include <basics.h>
#include <cpuid2.h>
#include <heap.h>
#include <drivers/pc-speaker.h>
#include <drivers/rtl8139.h>
#include <boot-logo.h>
#include <versions.h>

/**
 * @brief The memory address pointer where the kernel ends.
 * 
 */
extern int64* kend;

/**
 * @brief Halt and catch fire function.
 * 
 */
void hcf();

/**
 * @brief The clear interrupts command for all architectures.
 * 
 */
void clear_interrupts();

/**
 * @brief It uses while loops instead of assembly's halt,
 * Good for Userland
 * 
 */
void high_level_halt();

/**
 * @brief Halt and catch fire function but doesn't print any text.
 * 
 */
void hcf2();

/**
 * @brief An integer value which stores terminal's rows
 * 
 */
extern int terminal_rows;

/**
 * @brief An integer value which stores terminal's columns
 * 
 */
extern int terminal_columns;

extern int fb_width;
extern int fb_height;

/**
 * @brief The main kernel function
 * @attention main() to something else, make sure to change the linker script accordingly.
 */
void main(void);