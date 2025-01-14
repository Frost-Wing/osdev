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

extern char scancode_to_char_mapping[];

/**
 * @brief Converts the scancode to character
 * 
 * @param scancode usually inb(0x60);
 * @return [char] Appropriate character to be displayed
 */
char scancode_to_char(int scancode, bool uppercase);

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