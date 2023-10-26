
/**
 * @file fpu.c
 * @author Kevin Lange
 * @brief Floating point arithmetics
 * @version Unknown
 * @date Unknown
 * 
 * @copyright Copyright (C) 2011 Kevin Lange 
 * 
 */

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Set the FPU Control Word
 * 
 * @param cw 
 */
void set_fpu_cw(const uint16_t cw) {
	asm volatile("fldcw %0" :: "m"(cw));
}

/**
 * @brief Enables the Floating point arithmetic unit for x86-64
 * 
 */
void enable_fpu() {
	info("Enabling floating-point arithmetic unit!", __FILE__);
	size_t cr4;
	asm volatile ("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= 0x200;
	asm volatile ("mov %0, %%cr4" :: "r"(cr4));
	set_fpu_cw(0x37F);
    done("Enabled floating-point arithmetic unit!", __FILE__);
}
