/**
 * @file meltdown.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The headers for meltdown.c
 * @version 0.1
 * @date 2023-11-10
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stddef.h>
#include <stdint.h>
#include <basics.h>
#include <kernel.h>
#include <opengl/glbackend.h>

/**
 * @brief The Meltdown (Panic) Screen
 * 
 * @param message The Reason to cause a panic
 * @param file Handler's file
 * @param line Handler's line
 * @param error_code Error codes from registers
 * @param cr2
 * @param int_no
 */
void meltdown_screen(cstring message, cstring file, int line, int64 error_code, int64 cr2, int64 int_no);