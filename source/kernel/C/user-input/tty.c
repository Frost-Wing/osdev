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
#include <keyboard.h>

typedef struct {
    ring_buffer_t cooked_rb;
    char cooked_storage[TTY_COOKED_MAX];
    char line_buf[TTY_LINE_MAX];
    size_t line_len;
    bool eof;
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
        tty->eof = false;
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

    /* Ctrl+D: publish the pending line and signal EOF to the reader */
    if (c == 4) {
        for (size_t i = 0; i < tty->line_len; i++)
            tty_push_cooked(tty, tty->line_buf[i]);
        tty->line_len = 0;
        tty->eof = true;
        spinlock_unlock(&tty->lock);
        return;
    }

    /* Ignore other control chars */
    if (c != '\t' && ((unsigned char)c < 32 || (unsigned char)c > 126)) {
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

void tty_input_key(int key) {
    const char *seq = NULL;
    switch (key) {
        case CUR_UP:    seq = "\033[A";  break;
        case CUR_DOWN:  seq = "\033[B";  break;
        case CUR_RIGHT: seq = "\033[C";  break;
        case CUR_LEFT:  seq = "\033[D";  break;
        case KEY_HOME:  seq = "\033[H";  break;
        case KEY_END:   seq = "\033[F";  break;
        case KEY_DEL:   seq = "\033[3~"; break;
        case KEY_PGUP:  seq = "\033[5~"; break;
        case KEY_PGDN:  seq = "\033[6~"; break;
    }

    if (!seq) {
        tty_input_char((char)key);
        return;
    }

    tty_t *tty = &ttys[tty_active_index()];
    spinlock_lock(&tty->lock);
    /* No line editing in canonical mode, so drop the sequence rather than echo junk. */
    if (!(tty->termios.c_lflag & LINUX_ICANON)) {
        for (const char *p = seq; *p; p++)
            tty_push_cooked(tty, *p);
    }
    spinlock_unlock(&tty->lock);
}

extern volatile uint64_t pit_ticks;

int tty_read(char *buf, uint64_t count) {
    if (!buf || count == 0)
        return 0;

    tty_t *tty = &ttys[multitasking_current_tty()];
    uint64_t read = 0;
    static uint64_t last_tick = 0;

    for (;;) {
        char c = 0;
        bool got = false, eof = false;

        spinlock_lock(&tty->lock);
        bool canon = (tty->termios.c_lflag & LINUX_ICANON) != 0;
        uint8_t vmin = tty->termios.c_cc[LINUX_VMIN];
        if (rb_pop(&tty->cooked_rb, &c) == 0) {
            got = true;
        } else if (tty->eof) {
            tty->eof = false;
            eof = true;
        }
        spinlock_unlock(&tty->lock);

        if (got) {
            buf[read++] = c;
            if (read >= count)
                break;
            if (canon && c == '\n')
                break;
            continue;
        }

        if (eof)
            break;                      /* returns 0 if nothing was read: EOF */
        if (!canon && read > 0)
            break;                      /* raw mode: return what we have */
        if (!canon && vmin == 0)
            break;                      /* VMIN=0: poll, don't block */

        if (pit_ticks != last_tick) {
            last_tick = pit_ticks;
            multitasking_on_pit_tick(last_tick);
        }
        asm volatile("hlt");
    }

    return (int)read;
}

void tty_flush_input(void) {
    tty_t *tty = &ttys[multitasking_current_tty()];
    spinlock_lock(&tty->lock);
    rb_clear(&tty->cooked_rb);
    tty->line_len = 0;
    spinlock_unlock(&tty->lock);
}
