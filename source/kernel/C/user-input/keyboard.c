/**
 * @file keyboard.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The code for keyboard handlers
 * @version 0.1
 * @date 2023-11-14
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */

#include <basics.h>
#include <stdbool.h>
#include <hal.h>
#include <isr.h>

bool enable_keyboard = yes;

/**
 * @brief All the chars for specific scan codes
 * 
 */
char scancode_to_char_mapping[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0, // 0-15
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0, 'a', 's', // 16-31
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', // 32-47
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', // 48-57
};

/**
 * @brief Converts the scancode to character
 * 
 * @param scancode usually inb(0x60);
 * @return [char] Appropriate character to be displayed
 */
char scancode_to_char(unsigned char scancode) {

    if (scancode < sizeof(scancode_to_char_mapping)) {
        return scancode_to_char_mapping[scancode];
    } else {
        return 0; // Character not found
    }
}

/**
 * @brief This is a function that is ran even when the sleep() function is called
 * 
 */
void process_keyboard(InterruptFrame* frame){
    char c = '\0';
    if(!enable_keyboard) return;

    int keyboard = inb(0x60);
    if(keyboard == 0x44){ // F10 Key
        enable_keyboard = no;
        shutdown();
    }
    if(keyboard == 0x1C){ // Enter
        print("\n");
    }
    if(keyboard == 0x43){ // F9 Key
        enable_keyboard = no;
        acpi_reboot();
    }
    if(keyboard == 0x42){ // F8 Key
        display_time();
    }
    if(keyboard < sizeof(scancode_to_char_mapping)) {
        c = scancode_to_char(keyboard);
        print(&c);
    }
}

void basic_delay(){
    for(int i = 0; i < 10000000*5; i++) {asm("nop");}
}