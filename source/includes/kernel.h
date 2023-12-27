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
#include <strings2.h>
#include <math/fpu.h>
#include <sse.h>
#include <debugger.h>
#include <drivers/serial.h>
#include <basics.h>
#include <cpuid2.h>
#include <heap.h>
#include <drivers/pc-speaker.h>
#include <drivers/rtl8139.h>
#include <versions.h>
#include <cc-asm.h>
#include <secure-boot.h>
#include <paging.h>
#include <algorithms/hashing.h>
#include <keyboard.h>

/**
 * @brief The memory address pointer where the kernel ends.
 * 
 */
extern int64* kend;

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

/**
 * @brief An integer value which stores the framebuffer's (display) width
 * 
 */
extern int fb_width;

/**
 * @brief An integer value which stores the framebuffer's (display) height
 * 
 */
extern int fb_height;

/**
 * @brief The main kernel function
 * @attention main() to something else, make sure to change the linker script accordingly.
 */
void main(void);