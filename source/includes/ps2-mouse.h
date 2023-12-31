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
#include <debugger.h>
#include <opengl/glbackend.h>
#include <isr.h>
#include <hal.h>

#define PS2_left_button   0b00000001
#define PS2_middle_button 0b00000100
#define PS2_right_button  0b00000010
#define PS2_x             0b00010000
#define PS2_y             0b00100000
#define PS2_x_overflow    0b01000000
#define PS2_y_overflow    0b10000000

#define MOUSE_BUTTON_LEFT PS2_left_button
#define MOUSE_BUTTON_RIGHT PS2_right_button
#define MOUSE_BUTTON_MIDDLE PS2_middle_button

#define MOUSE_BUTTON_PRESS 1
#define MOUSE_BUTTON_RELEASE 0

extern const bool mouse_cursor[];

/**
 * @brief Type definition for mouse movement handler functions
 */
typedef void(*MouseMovementHandler)(int64_t xRel, int64_t yRel);

/**
 * @brief Type definition for mouse button handler functions
 */
typedef void(*MouseButtonHandler)(uint8_t button, uint8_t action);

/**
 * @brief Set the mouse movement handler function
 * 
 * @author GAMINGNOOBdev (https://github.com/GAMINGNOOBdev)
 * 
 * @param handler Mouse movement handler function
 */
void SetMouseMovementHandler(MouseMovementHandler handler);

/**
 * @brief Set the mouse button handler function
 * 
 * @author GAMINGNOOBdev (https://github.com/GAMINGNOOBdev)
 * 
 * @param handler Mouse button handler function
 */
void SetMouseButtonHandler(MouseButtonHandler handler);

/**
 * @brief Get the mouse position
 * 
 * @author GAMINGNOOBdev (https://github.com/GAMINGNOOBdev)
 * 
 * @returns The mouse position
 */
ivec2 GetMousePosition();

/**
 * @brief Get the last mouse position
 * 
 * @author GAMINGNOOBdev (https://github.com/GAMINGNOOBdev)
 * 
 * @returns The last mouse position
 */
ivec2 GetLastMousePosition();

/**
 * @brief Handles the mouse's 
 * 
 * @param frame 
 */
void process_mouse(InterruptFrame* frame);