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
#include <stdint.h>

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
uint8_t modifiers = 0;

/**
 * @brief This is a function that is ran even when the sleep() function is called
 * 
 */
void process_keyboard(InterruptFrame* frame){
    if(!enable_keyboard) return;

    int data = inb(0x60);
    
    exit_interrupt:
    c = '\0';
    outb(0x20, 0x20); // End PIC Master
}

uint8_t getmodifiers()
{
    return modifiers;
}

uint8_t get_extended_keyboard_data() // only called if last data was the 0xE0 extended byte
{
    while (1)
        if (inb(0x64) & 1)
            break;

    uint8_t data = inb(0x60);
    switch(data)
    {
    case 0x38: //RALT
        modifiers |= MOD_RALT;
        outb(0x20, 0x20);
        return 0;
    case 0xB8: //RALT release
        modifiers &= ~MOD_RALT;
        outb(0x20, 0x20);
        return 0;
    case 0x1D: //RCTRL
        modifiers |= MOD_RCTRL;
        outb(0x20, 0x20);
        return 0;
    case 0x9D: //RCTRL release
        modifiers &= ~MOD_RCTRL;
        outb(0x20, 0x20);
        return 0;
    /* TODO: cursor movement currently breaks the printing system
    case 0x48: //CURSOR UP
        outb(0x20, 0x20);
        return CUR_UP;
    case 0x50: //CURSOR DOWN
        outb(0x20, 0x20);
        return CUR_DOWN;
    case 0x4B: //CURSOR LEFT
        outb(0x20, 0x20);
        return CUR_LEFT;
    case 0x4D: //CURSOR RIGHT
        outb(0x20, 0x20);
        return CUR_RIGHT;*/
    }

    return 0;
}

uint8_t get_keyboard_data()
{
    while (1)
        if (inb(0x64) & 1)
            break;

    uint8_t data = inb(0x60);
    if (data == 0xE0)
    {
        while (1)
        {
            data = get_extended_keyboard_data();
            if (data != -1)
                break;
        }
        return 0;
    }

    switch(data)
    {
    case 0x2A: //LSHIFT
    case 0x36: //RSHIFT
        modifiers |= (data == 0x2A) ? MOD_LSHIFT : MOD_RSHIFT;
        outb(0x20, 0x20);
        return 0;
    case 0xAA: //LSHIFT release
    case 0xB6: //RSHIFT release
        modifiers &= ~((data == 0xAA) ? MOD_LSHIFT : MOD_RSHIFT);
        outb(0x20, 0x20);
        return 0;
    case 0x38: //LALT
        modifiers |= MOD_LALT;
        outb(0x20, 0x20);
        return 0;
    case 0xB8: //LALT release
        modifiers &= ~MOD_LALT;
        outb(0x20, 0x20);
        return 0;
    case 0x1D: //LCTRL
        modifiers |= MOD_LCTRL;
        outb(0x20, 0x20);
        return 0;
    case 0x9D: //LCTRL release
        modifiers &= ~MOD_LCTRL;
        outb(0x20, 0x20);
        return 0;
    case 0x3A:
        modifiers ^= MOD_CAPSLOCK;
        outb(0x20, 0x20);
        return 0;
    case 0x45:
        modifiers ^= MOD_CAPSLOCK;
        outb(0x20, 0x20);
        return 0;
    case 0x0E:
        return data;
    case 0x1C:
        return data;
    }

    if(data > 0x80)
        return 0;

end:
    // Send EOI to PIC
    outb(0x20, 0x20);
    return data;
}

uint8_t getc() {
    uint8_t data = get_keyboard_data();
    if (data == 0)
        return 0;
    if(data == 0x1C)
        return '\n';
    if(data == 0x0E)
        return '\b';
    
    char c = scancode_to_char(data, modifiers & MOD_SHIFT);
    return c;
}