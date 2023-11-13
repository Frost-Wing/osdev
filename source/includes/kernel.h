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
#include <typescript-loader.h>
#include <debugger.h>
#include <drivers/serial.h>
#include <basics.h>
#include <cpuid2.h>
#include <heap.h>
#include <drivers/pc-speaker.h>
#include <drivers/rtl8139.h>
#include <boot-logo.h>

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
 * @brief The fake or pseudo frame buffer in the memory which will be queued for next frame update
 * 
 */
extern int64* back_buffer;

/**
 * @brief Direct pointer to framebuffer->address. It doesn't queue, directly writes to the memory
 * 
 */
extern int64* front_buffer;