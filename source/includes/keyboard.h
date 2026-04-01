/**
 * @file keyboard.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The PS/2 Keyboard interface code.
 * @version 0.1
 * @date 2026-03-20
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <basics.h>
#include <stdbool.h>
#include <hal.h>
#include <isr.h>

#define MOD_LCTRL       0b00000001
#define MOD_RCTRL       0b00000010
#define MOD_CTRL        0b00000011
#define MOD_LSHIFT      0b00000100
#define MOD_RSHIFT      0b00001000
#define MOD_SHIFT       0b01001100
#define MOD_LALT        0b00010000
#define MOD_RALT        0b00100000
#define MOD_ALT         0b00110000
#define MOD_CAPSLOCK    0b01000000
#define MOD_NUMLOCK     0b10000000

#define CUR_UP          -1
#define CUR_DOWN        -2
#define CUR_LEFT        -3
#define CUR_RIGHT       -4

#define KB_BUFFER_SIZE  256

/**
 * @brief Converts the scancode to character
 * 
 * @param scancode usually inb(0x60);
 * @return [char] Appropriate character to be displayed
 */
char scancode_to_char(int scancode, bool uppercase);

/**
 * @brief Initalizes the RingBuffer to store the characters.
 * 
 */
void keyboard_init();

/**
 * @brief This is a function that is ran even when the sleep() function is called
 * 
 */
void process_keyboard(InterruptFrame* frame);

/**
 * @brief Gets the current modifiers (like lshift, rshift, etc.)
 */
uint8_t getmodifiers();

/**
 * @brief Gets the last pressed char.
 * 
 * @return [uint8_t] Last scancode
 */
uint8_t getc();

/**
 * @brief Non-blocking getc from keyboard buffer
 * 
 * @return int Character if available, 0 if no input
 */
int getc_nonblock();

/**
 * @brief Non-blocking read directly from the PS/2 controller.
 * 
 * @return int Character if available, 0 if no input or only a modifier event
 */
int kgetc_nonblock();

int handle_char_from_scancode(uint8_t data);

#endif