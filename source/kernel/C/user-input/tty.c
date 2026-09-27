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

#include <graphics.h>
#include <fb.h>
#include <heap.h>
#include <multitasking.h>
#include <ringbuffer.h>
#include <sys/termios.h>
#include <tty.h>

typedef struct {
    ring_buffer_t cooked_rb;
    char cooked_storage[TTY_COOKED_MAX];
    char line_buf[TTY_LINE_MAX];
    size_t line_len;
    linux_termios_t termios;
    struct flanterm_context *display;
    spinlock_t lock;
} tty_t;

static tty_t ttys[TTY_COUNT];
static volatile uint8_t active_tty;

void tty_init(void) {
    for (uint8_t i = 0; i < TTY_COUNT; ++i) {
        tty_t *tty = &ttys[i];
        rb_init(&tty->cooked_rb, tty->cooked_storage, TTY_COOKED_MAX, sizeof(char));
        tty->termios = (linux_termios_t){
            .c_iflag = LINUX_ICRNL | LINUX_IXON,
            .c_oflag = LINUX_OPOST | LINUX_ONLCR,
            .c_cflag = LINUX_CREAD | LINUX_CS8,
            .c_lflag = LINUX_ISIG | LINUX_ICANON |
                       LINUX_ECHO | LINUX_ECHOE |
                       LINUX_ECHOK | LINUX_IEXTEN,
        };

        tty->termios.c_cc[LINUX_VMIN] = 1;
        tty->termios.c_cc[LINUX_VTIME] = 0;
        tty->line_len = 0;
        tty->display = NULL;
        tty->lock = (spinlock_t)SPINLOCK_INITIALIZER;
    }
    active_tty = 0;
}

static void tty_push_cooked(tty_t *tty, char c) {
    if (rb_push(&tty->cooked_rb, &c) != 0) {
        char drop;
        rb_pop(&tty->cooked_rb, &drop);
        rb_push(&tty->cooked_rb, &c);
    }
}

static void tty_heap_free(void *ptr, size_t size) {
    (void)size;
    kfree(ptr);
}

bool tty_init_terminals(struct flanterm_context *default_terminal,
                        uint32_t *framebuffer, size_t width, size_t height,
                        size_t pitch) {
    if (!default_terminal || !framebuffer)
        return false;

    ttys[0].display = default_terminal;
    for (uint8_t i = 1; i < TTY_COUNT; ++i) {
        ttys[i].display = flanterm_fb_init(kmalloc, tty_heap_free, framebuffer,
            width, height, pitch,
#ifndef FLANTERM_FB_DISABLE_CANVAS
            NULL,
#endif
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 1, 1, 1, 0);
        if (!ttys[i].display)
            return false;
    }
    return tty_switch(0);
}

uint8_t tty_active_index(void) {
    return __atomic_load_n(&active_tty, __ATOMIC_ACQUIRE);
}

bool tty_switch(uint8_t index) {
    if (index >= TTY_COUNT || !ttys[index].display)
        return false;

    __atomic_store_n(&active_tty, index, __ATOMIC_RELEASE);
    terminal_switch_context(ttys[index].display);
    return true;
}

void tty_input_char(char c) {
    if (c == '\0')
        return;

    tty_t *tty = &ttys[tty_active_index()];
    spinlock_lock(&tty->lock);

    /* CR -> NL if enabled */
    if (c == '\r' && (tty->termios.c_iflag & LINUX_ICRNL))
        c = '\n';

    /* -------- RAW MODE -------- */
    if (!(tty->termios.c_lflag & LINUX_ICANON)) {
        tty_push_cooked(tty, c);

        if (tty->termios.c_lflag & LINUX_ECHO)
            putc(c);
        spinlock_unlock(&tty->lock);
        return;
    }

    /* -------- CANONICAL MODE -------- */

    /* Backspace */
    if (c == '\b' || c == 127) {
        if (tty->line_len == 0) {
            spinlock_unlock(&tty->lock);
            return;
        }

        tty->line_len--;

        if (tty->termios.c_lflag & LINUX_ECHO)
            putc('\b');
        spinlock_unlock(&tty->lock);
        return;
    }

    /* Enter */
    if (c == '\n') {
        if (tty->line_len < TTY_LINE_MAX)
            tty->line_buf[tty->line_len++] = '\n';

        /* Publish the whole line */
        for (size_t i = 0; i < tty->line_len; i++)
            tty_push_cooked(tty, tty->line_buf[i]);

        if (tty->termios.c_lflag & LINUX_ECHO)
            putc('\n');

        tty->line_len = 0;
        spinlock_unlock(&tty->lock);
        return;
    }

    /* Ignore other control chars */
    if ((unsigned char)c < 32 || (unsigned char)c > 126) {
        spinlock_unlock(&tty->lock);
        return;
    }

    if (tty->line_len >= TTY_LINE_MAX - 1) {
        spinlock_unlock(&tty->lock);
        return;
    }

    tty->line_buf[tty->line_len++] = c;

    if (tty->termios.c_lflag & LINUX_ECHO)
        putc(c);
    spinlock_unlock(&tty->lock);
}

extern volatile uint64_t pit_ticks;

int tty_read(char *buf, uint64_t count) {
    if (!buf || count == 0)
        return 0;

    tty_t *tty = &ttys[tty_active_index()];
    uint64_t read = 0;
    static uint64_t last_tick = 0;

    while (read < count) {
        char c;
        while (true) {
            spinlock_lock(&tty->lock);
            int empty = rb_pop(&tty->cooked_rb, &c);
            spinlock_unlock(&tty->lock);
            if (empty == 0)
                break;
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
    tty_t *tty = &ttys[tty_active_index()];
    spinlock_lock(&tty->lock);
    rb_clear(&tty->cooked_rb);
    tty->line_len = 0;
    spinlock_unlock(&tty->lock);
}
