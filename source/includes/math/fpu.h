/**
 * @file fpu.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Header for FPU
 * @version 0.1
 * @date 2023-10-26
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Set the FPU Control Word
 * 
 * @param cw 
 */
void set_fpu_cw(const uint16_t cw);

/**
 * @brief Enables the Floating point arithmetic unit for x86-64
 * 
 */
void enable_fpu();