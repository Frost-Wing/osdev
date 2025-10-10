/**
 * @file sse.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief SSE Header file
 * @version 0.1
 * @date 2023-10-27
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */

#ifndef SSE_H
#define SSE_H

#include <basics.h>
#include <graphics.h>

extern char fxsave_region[512] __attribute__((aligned(16)));

/**
 * @brief Loads the SEE fully with fxsave
 * 
 */
void load_complete_sse();

/**
 * @brief Checks if CPU is compatible with SSE
 * 
 */
void check_sse();

#endif