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

#include <graphics.h>
#include <heap.h>
#include <keyboard.h>
#include <stdint.h>
#include <ringbuffer.h>

bool enable_keyboard = yes;
static ring_buffer_t kb_rb;
static uint8_t kb_storage[KB_BUFFER_SIZE];

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

void keyboard_init() {
    rb_init(&kb_rb, kb_storage, KB_BUFFER_SIZE, sizeof(uint8_t));
}

bool shift = no;
uint8_t modifiers = 0;
void process_keyboard(InterruptFrame* frame){
    if(!enable_keyboard) {
        outb(0x20, 0x20);
        return;
    }

    int c = kgetc_nonblock();

    if (c > 0) {
        uint8_t key = (uint8_t)c;
        rb_push(&kb_rb, &key);
    }

    // EOI
    outb(0x20, 0x20);
}

uint8_t getmodifiers()
{
    return modifiers;
}

uint8_t getc() {
    uint8_t c;

    for (;;) {
        if (rb_pop(&kb_rb, &c) == 0)
            return c;

        asm volatile ("hlt");
    }
}

int getc_nonblock() {
    uint8_t c;

    if (rb_pop(&kb_rb, &c) == 0)
        return c;

    return 0; // no key available
}

int kgetc_nonblock() {

    if (!(inb(0x64) & 1))
        return 0;

    uint8_t data = inb(0x60);

    // Extended prefix
    if (data == 0xE0) {
        uint8_t ext = inb(0x60);

        switch (ext)
        {
        case 0x38: // RALT press
            modifiers |= MOD_RALT;
            return 0;
        case 0xB8: // RALT release
            modifiers &= ~MOD_RALT;
            return 0;

        case 0x1D: // RCTRL press
            modifiers |= MOD_RCTRL;
            return 0;
        case 0x9D: // RCTRL release
            modifiers &= ~MOD_RCTRL;
            return 0;
        }

        return 0;
    }

    switch (data)
    {
        // -------- SHIFT --------
        case 0x2A: // LSHIFT press
            modifiers |= MOD_LSHIFT;
            return 0;
        case 0x36: // RSHIFT press
            modifiers |= MOD_RSHIFT;
            return 0;

        case 0xAA: // LSHIFT release
            modifiers &= ~MOD_LSHIFT;
            return 0;
        case 0xB6: // RSHIFT release
            modifiers &= ~MOD_RSHIFT;
            return 0;

        // -------- CTRL --------
        case 0x1D: // LCTRL press
            modifiers |= MOD_LCTRL;
            return 0;
        case 0x9D: // LCTRL release
            modifiers &= ~MOD_LCTRL;
            return 0;

        // -------- ALT --------
        case 0x38: // LALT press
            modifiers |= MOD_LALT;
            return 0;
        case 0xB8: // LALT release
            modifiers &= ~MOD_LALT;
            return 0;

        // -------- CAPS LOCK --------
        case 0x3A:
            modifiers ^= MOD_CAPSLOCK;
            return 0;

        // -------- NUM LOCK --------
        case 0x45:
            modifiers ^= MOD_NUMLOCK;
            return 0;

        // -------- SPECIAL KEYS --------
        case 0x1C:
            return '\n';

        case 0x0E:
            return '\b';
    }

    // Ignore key releases
    if (data > 0x80)
        return 0;

    // -------- CHARACTER OUTPUT --------

    int use_upper = (modifiers & MOD_SHIFT) || (modifiers & MOD_CAPSLOCK);

    char c = scancode_to_char(data, use_upper);

    // Filter non-printable
    if (c < 32 && c != '\n' && c != '\b')
        return 0;

    return c;
}