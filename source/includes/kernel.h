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
#include <acpi-shutdown.h>
#include <acpi.h>
#include <algorithms/hashing.h>
#include <basics.h>
#include <cc-asm.h>
#include <cpuid2.h>
#include <debugger.h>
#include <drivers/pc-speaker.h>
#include <drivers/rtl8139.h>
#include <drivers/serial.h>
#include <executables/fwde.h>
#include <fb.h>
#include <fdlfcn.h>
#include <flanterm/flanterm.h>
#include <graphics.h>
#include <hal.h>
#include <heap.h>
#include <image/targa.h>
#include <keyboard.h>
#include <limine.h>
#include <linkedlist.h>
#include <math/fpu.h>
#include <memory.h>
#include <opengl/glbackend.h>
#include <opengl/glcontext.h>
#include <paging.h>
#include <pci.h>
#include <ps2-mouse.h>
#include <secure-boot.h>
#include <sse.h>
#include <stddef.h>
#include <stdint.h>
#include <stream.h>
#include <strings.h>
#include <versions.h>

/**
 * @brief  Long story short: linker is a mole-rat and gives virtual addresses. But we asked the linker to allocate at this address, so we are spoofing its "security" to get the real memory address of kernel start and end.
 *
 */
#define KERNEL_OFFSET 0xffffffff00000000

/**
 * @brief The memory address where the kernel starts.
 *
 */
extern uint8 kstart[];

/**
 * @brief The memory address where the kernel ends.
 *
 */
extern uint8 kend[];

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
extern uint64 fb_width;

/**
 * @brief An integer value which stores the framebuffer's (display) height
 *
 */
extern uint64 fb_height;

extern uint64 *wm_addr;

/**
 * @brief The main kernel function
 * @attention main() to something else, make sure to change the linker script accordingly.
 */
void main(void);
void shutdown(void);
void reboot(void);
