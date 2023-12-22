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

#include <keyboard.h>

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

char shifted_scancode_to_char_mapping[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0, // 0-15
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0, 'A', 'S', // 16-31
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0, '|', 'Z', 'X', 'C', 'V', // 32-47
    'B', 'N', 'M', '<', '>', '\?', 0, '*', 0, ' ', // 48-57
};

/**
 * @brief Converts the scancode to character
 * 
 * @param scancode usually inb(0x60);
 * @return [char] Appropriate character to be displayed
 */
char scancode_to_char(int scancode, bool uppercase) {
    if(scancode > 58) return '\0';
    
    if(uppercase){
        char to_send = shifted_scancode_to_char_mapping[scancode];
        if((int)to_send == (int)0){
            return '\0';
        }else{
            return to_send;
        }
    }else{
        char to_send = scancode_to_char_mapping[scancode];
        if((int)to_send == (int)0){
            return '\0';
        }else{
            return to_send;
        }
    }
    
    return '\0';
}

char c = '\0';
bool shift = no;
/**
 * @brief This is a function that is ran even when the sleep() function is called
 * 
 */
void process_keyboard(InterruptFrame* frame){
    if(!enable_keyboard) return;

    int keyboard = inb(0x60);
    if(keyboard == 0x44){ // F10 Key
        enable_keyboard = no;
        shutdown();
        goto exit_interrupt;
    }
    if(keyboard == 0x1C){ // Enter
        print("\n");
        goto exit_interrupt;
    }
    if(keyboard == 0x43){ // F9 Key
        enable_keyboard = no;
        acpi_reboot();
        goto exit_interrupt;
    }
    if(keyboard == 0x0E){ // Backspace Key
        print("\b \b");
        goto exit_interrupt;
    }

    if(keyboard == 0x2A || keyboard == 0x36){ // [Left Shift || Right Shift] pressed
        shift = yes;
    }

    if(keyboard == 0xB6 || keyboard == 0xAA){
        shift = no;
    }

    c = scancode_to_char(keyboard, shift);

    if(c != '\0') print(&c);

    exit_interrupt:
    c = '\0';
    outb(0x20, 0x20); // End PIC Master
}