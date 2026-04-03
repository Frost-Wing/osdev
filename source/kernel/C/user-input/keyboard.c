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
#include <tty.h>

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

void process_keyboard(InterruptFrame* frame)
{
    if (!enable_keyboard) {
        outb(0x20, 0x20);
        return;
    }

    uint8_t scancode = inb(0x60);

    rb_push(&kb_rb, &scancode);

    int c = handle_char_from_scancode(scancode);
    if (c != 0)
        tty_input_char((char)c);

    outb(0x20, 0x20);
}

uint8_t getmodifiers()
{
    return modifiers;
}

extern volatile int pit_ticks;
uint8_t getc()
{
    uint8_t sc;
    static uint64_t last_tick = 0;

    for (;;) {
        if (rb_pop(&kb_rb, &sc) == 0) {
            return handle_char_from_scancode(sc);
        }

        if (pit_ticks != last_tick) {
            last_tick = pit_ticks;
            multitasking_on_pit_tick(last_tick);
        }

        asm volatile("hlt");
    }
}

int getc_nonblock() {
    uint8_t sc;

    if (rb_pop(&kb_rb, &sc) == 0) {
        return handle_char_from_scancode(sc);
    }

    return 0; // no key available
}

void keyboard_flush_buffer(void) {
    rb_clear(&kb_rb);
}

static bool extended = false;
int handle_char_from_scancode(uint8_t data)
{
    // -------- Extended scancode handling --------
    if (data == 0xE0) {
        extended = true;
        return 0;
    }

    // -------- Key release handling --------
    if (data & 0x80) {
        uint8_t key = data & 0x7F;

        switch (key) {
            case 0x2A: // LSHIFT release
                modifiers &= ~MOD_LSHIFT;
                return 0;
            case 0x36: // RSHIFT release
                modifiers &= ~MOD_RSHIFT;
                return 0;

            case 0x1D: // CTRL release
                modifiers &= ~MOD_LCTRL;
                return 0;

            case 0x38: // ALT release
                modifiers &= ~MOD_LALT;
                return 0;
        }

        return 0;
    }

    // -------- Extended keys (only press matters here) --------
    if (extended) {
        extended = false;

        switch (data) {
            case 0x1D: // Right CTRL press
                modifiers |= MOD_RCTRL;
                return 0;

            case 0x38: // Right ALT press
                modifiers |= MOD_RALT;
                return 0;

            // You can extend more extended keys here (arrows, etc.)
        }

        return 0;
    }

    // -------- Modifier key presses --------
    switch (data) {
        case 0x2A: // LSHIFT press
            modifiers |= MOD_LSHIFT;
            return 0;

        case 0x36: // RSHIFT press
            modifiers |= MOD_RSHIFT;
            return 0;

        case 0x1D: // LCTRL press
            modifiers |= MOD_LCTRL;
            return 0;

        case 0x38: // LALT press
            modifiers |= MOD_LALT;
            return 0;

        case 0x3A: // CAPSLOCK toggle
            modifiers ^= MOD_CAPSLOCK;
            return 0;

        case 0x45: // NUMLOCK toggle
            modifiers ^= MOD_NUMLOCK;
            return 0;

        case 0x1C: // Enter
            return '\n';

        case 0x0E: // Backspace
            return '\b';
    }

    // -------- Ignore non-character keys --------
    if (data > 0x58)
        return 0;

    // -------- Character conversion --------
    bool use_shift = (modifiers & (MOD_LSHIFT | MOD_RSHIFT));

    // Caps lock XOR shift logic
    bool uppercase = use_shift ^ (modifiers & MOD_CAPSLOCK);

    char c = scancode_to_char(data, uppercase);

    if (c == 0)
        return 0;

    return c;
}
