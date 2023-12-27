/**
 * @file pit.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The code for PIT
 * @version 0.1
 * @date 2023-12-27
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <pit.h>

void process_pit(InterruptFrame* frame){
    // nothing yet
    outb(0x20, 0x20);
}