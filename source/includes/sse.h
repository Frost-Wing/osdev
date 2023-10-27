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
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

extern char fxsave_region[512] __attribute__((aligned(16)));

void load_complete_sse();
void check_sse();