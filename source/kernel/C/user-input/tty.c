/**
 * @file tty.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Source code for kernel's terminal output management. (TTY)
 * @version 0.1
 * @date 2026-04-02
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */

#include <tty.h>
#include <graphics.h>
#include <ringbuffer.h>

static ring_buffer_t cooked_rb;
static char cooked_storage[TTY_COOKED_MAX];

static char line_buf[TTY_LINE_MAX];
static size_t line_len = 0;

void tty_init(void) {
    rb_init(&cooked_rb, cooked_storage, TTY_COOKED_MAX, sizeof(char));
    line_len = 0;
}

static void tty_push_cooked(char c) {
    if (rb_push(&cooked_rb, &c) != 0) {
        char drop;
        rb_pop(&cooked_rb, &drop);
        rb_push(&cooked_rb, &c);
    }
}

void tty_input_char(char c) {
    if (c == '\0')
        return;

    if (c == '\b') {
        if (line_len == 0)
            return;

        line_len--;
        putc('\b');
        return;
    }

    if (c == '\n') {
        if (line_len < TTY_LINE_MAX)
            line_buf[line_len++] = '\n';

        for (size_t i = 0; i < line_len; ++i)
            tty_push_cooked(line_buf[i]);

        putc('\n');
        line_len = 0;
        return;
    }

    if (c < 32 || c > 126)
        return;

    if (line_len >= TTY_LINE_MAX - 1)
        return;

    line_buf[line_len++] = c;
    putc(c);
}

extern volatile int pit_ticks;
int tty_read(char* buf, uint64_t count) {
    if (!buf || count == 0)
        return 0;

    uint64_t read = 0;
    static uint64_t last_tick = 0;

    while (read < count) {
        char c;
        while (rb_pop(&cooked_rb, &c) != 0) {
            if (pit_ticks != last_tick) {
                last_tick = pit_ticks;
                multitasking_on_pit_tick(last_tick);
            }
            asm volatile("hlt");
        }

        buf[read++] = c;

        if (c == '\n')
            break;
    }

    return (int)read;
}

void tty_flush_input(void) {
    rb_clear(&cooked_rb);
    line_len = 0;
}
