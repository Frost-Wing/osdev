/**
 * @file kernel.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Main kernel file, everything starts from here
 * @version 0.1
 * @date 2023-10-21
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <memory.h>
#include <strings.h>
 
// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent.
static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0
};

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

/**
 * @brief Halt and catch fire function.
 * 
 */
static void hcf(void) {
    for (;;) {
        #if defined (__x86_64__)
            asm ("hlt");
        #elif defined (__aarch64__) || defined (__riscv)
            asm ("wfi");
        #endif
    }
}

struct limine_terminal *terminal = NULL;

/**
 * @brief The main kernel function
 * renaming main() to something else, make sure to change the linker script accordingly.
 */
void main(void) {
    // Make sure we have a framebuffer to work with
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) hcf();

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];


    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    volatile uint32_t *fb_ptr = framebuffer->address;
    for (size_t i = 0; i < 400; i++) {
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }
    // We have no more process to handle.
    hcf(); // Doing this to avoid Reboot
}

/**
 * @brief The function to access the terminal from framebuffer and print text to.
 * 
 * @param msg (char[]) Message to be displayed
 * @deprecated This is dependent for old limine v5.x, now it is not needed.
 */
void print(char msg[]){
    terminal_request.response->write(terminal, msg, 8);
}