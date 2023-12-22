/**
 * @file ps2-mouse.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-12-22
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */

#include <basics.h>
#include <stdbool.h>
#include <hal.h>
#include <opengl/glbackend.h>
#include <isr.h>

#define PS2_left_button   0b00000001
#define PS2_middle_button 0b00000100
#define PS2_right_button  0b00000010
#define PS2_x             0b00010000
#define PS2_y             0b00100000
#define PS2_x_overflow    0b01000000
#define PS2_y_overflow    0b10000000

/**
 * @brief Handles the mouse's 
 * 
 * @param frame 
 */
void process_mouse(InterruptFrame* frame);