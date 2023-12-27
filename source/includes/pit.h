/**
 * @file pit.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Headers for PIT
 * @version 0.1
 * @date 2023-12-27
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>
#include <isr.h>
#include <hal.h>

/**
 * @brief Interrupt handlers for PIT
 * 
 * @param frame 
 */
void process_pit(InterruptFrame* frame);