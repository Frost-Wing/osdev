#include <basics.h>
#include <stdbool.h>
#include <hal.h>
#include <isr.h>

extern char scancode_to_char_mapping[];

/**
 * @brief Converts the scancode to character
 * 
 * @param scancode usually inb(0x60);
 * @return [char] Appropriate character to be displayed
 */
char scancode_to_char(unsigned char scancode);

/**
 * @brief This is a function that is ran even when the sleep() function is called
 * 
 */
void process_keyboard(InterruptFrame* frame);