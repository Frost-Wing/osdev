/**
 * @file tty.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Source code for kernel's terminal output management. (TTY)
 * @version 0.2
 * @date 2026-04-02
 *
 * Single-terminal build: one tty backed by the boot framebuffer console.
 * Nothing in here touches the heap.
 *
 * @copyright Copyright (c) Pradosh 2026
 *
 */

#include <flanterm/flanterm.h>
#include <graphics.h>
#include <fb.h>
#include <multitasking.h>
#include <ringbuffer.h>
#include <syscalls.h>
#include <sys/termios.h>
#include <tty.h>
#include <keyboard.h>
#include <xhci.h>

typedef struct {
    ring_buffer_t cooked_rb;
    char cooked_storage[TTY_COOKED_MAX];
    char line_buf[TTY_LINE_MAX];
    size_t line_len;
    bool eof;
    volatile int interrupt_exit_code;
    linux_termios_t termios;
    struct flanterm_context *display;
    spinlock_t lock;
} tty_t;

static tty_t ttys[TTY_COUNT];
static volatile uint8_t active_tty;

/* Any out-of-range index (e.g. TTY_INDEX_CURRENT) falls back to tty0. */
static inline tty_t *tty_get(uint8_t index) {
    return &ttys[index < TTY_COUNT ? index : 0];
}

static uint64_t tty_lock(tty_t *tty) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli"
        : "=r"(flags) : : "memory");
    spinlock_lock(&tty->lock);
    return flags;
}

static void tty_unlock(tty_t *tty, uint64_t flags) {
    spinlock_unlock(&tty->lock);
    __asm__ volatile("pushq %0; popfq"
        : : "r"(flags) : "memory", "cc");
}

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
        tty->termios.c_cc[LINUX_VINTR] = 3;
        tty->termios.c_cc[LINUX_VERASE] = 127;
        tty->termios.c_cc[LINUX_VEOF] = 4;
        tty->line_len = 0;
        tty->eof = false;
        tty->interrupt_exit_code = -1;
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

bool tty_init_terminals(struct flanterm_context *default_terminal,
    uint32_t *framebuffer, size_t width, size_t height,
    size_t pitch, const void *ssfn_font,
    size_t ssfn_font_size) {
    /* Kept for API compatibility; the boot terminal already exists. */
    (void)framebuffer;
    (void)width;
    (void)height;
    (void)pitch;
    (void)ssfn_font;
    (void)ssfn_font_size;

    if (!default_terminal)
        return false;

    ttys[0].display = default_terminal;
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

    tty_t *tty = tty_get(tty_active_index());
    uint64_t irq_flags = tty_lock(tty);

    /* CR -> NL if enabled */
    if (c == '\r' && (tty->termios.c_iflag & LINUX_ICRNL))
        c = '\n';

    if ((tty->termios.c_lflag & LINUX_ISIG) &&
        tty->termios.c_cc[LINUX_VINTR] != 0 &&
        (uint8_t)c == tty->termios.c_cc[LINUX_VINTR]) {
        tty->interrupt_exit_code = 130;
        tty->line_len = 0;
        if (!(tty->termios.c_lflag & LINUX_NOFLSH))
            rb_clear(&tty->cooked_rb);
        tty_unlock(tty, irq_flags);
        return;
    }

    bool eof_char = tty->termios.c_cc[LINUX_VEOF] != 0 &&
                    (uint8_t)c == tty->termios.c_cc[LINUX_VEOF];

    /* -------- RAW MODE -------- */
    if (!(tty->termios.c_lflag & LINUX_ICANON)) {
        tty_push_cooked(tty, c);

        if (tty->termios.c_lflag & LINUX_ECHO)
            putc(c);
        tty_unlock(tty, irq_flags);
        return;
    }

    /* -------- CANONICAL MODE -------- */

    /* Backspace */
    if (tty->termios.c_cc[LINUX_VERASE] != 0 &&
        (uint8_t)c == tty->termios.c_cc[LINUX_VERASE]) {
        if (tty->line_len == 0) {
            tty_unlock(tty, irq_flags);
            return;
        }

        tty->line_len--;

        if ((tty->termios.c_lflag & (LINUX_ECHO | LINUX_ECHOE)) ==
            (LINUX_ECHO | LINUX_ECHOE))
            putc('\b');
        else if (tty->termios.c_lflag & LINUX_ECHO)
            putc(c);
        tty_unlock(tty, irq_flags);
        return;
    }

    /* Enter */
    if (c == '\n') {
        if (tty->line_len < TTY_LINE_MAX)
            tty->line_buf[tty->line_len++] = '\n';

        /* Publish the whole line */
        for (size_t i = 0; i < tty->line_len; i++)
            tty_push_cooked(tty, tty->line_buf[i]);

        if (tty->termios.c_lflag & (LINUX_ECHO | LINUX_ECHONL))
            putc('\n');

        tty->line_len = 0;
        tty_unlock(tty, irq_flags);
        return;
    }

    /* Ctrl+D: publish the pending line and signal EOF to the reader */
    if (eof_char) {
        for (size_t i = 0; i < tty->line_len; i++)
            tty_push_cooked(tty, tty->line_buf[i]);
        tty->line_len = 0;
        tty->eof = true;
        tty_unlock(tty, irq_flags);
        return;
    }

    /* Ignore other control chars */
    if (c != '\t' && ((unsigned char)c < 32 || (unsigned char)c > 126)) {
        tty_unlock(tty, irq_flags);
        return;
    }

    if (tty->line_len >= TTY_LINE_MAX - 1) {
        tty_unlock(tty, irq_flags);
        return;
    }

    tty->line_buf[tty->line_len++] = c;

    if (tty->termios.c_lflag & LINUX_ECHO)
        putc(c);
    tty_unlock(tty, irq_flags);
}

void tty_input_key(int key) {
    if (key == KEY_PGUP) {
        terminal_scrollback_up();
        return;
    }
    if (key == KEY_PGDN) {
        terminal_scrollback_down();
        return;
    }

    terminal_scrollback_to_live();

    const char *seq = NULL;
    switch (key) {
        case CUR_UP:
            seq = "\033[A";
            break;
        case CUR_DOWN:
            seq = "\033[B";
            break;
        case CUR_RIGHT:
            seq = "\033[C";
            break;
        case CUR_LEFT:
            seq = "\033[D";
            break;
        case KEY_HOME:
            seq = "\033[H";
            break;
        case KEY_END:
            seq = "\033[F";
            break;
        case KEY_DEL:
            seq = "\033[3~";
            break;
    }

    if (!seq) {
        tty_input_char((char)key);
        return;
    }

    tty_t *tty = tty_get(tty_active_index());
    uint64_t irq_flags = tty_lock(tty);
    /* No line editing in canonical mode, so drop the sequence rather than echo junk. */
    if (!(tty->termios.c_lflag & LINUX_ICANON)) {
        for (const char *p = seq; *p; p++)
            tty_push_cooked(tty, *p);
    }
    tty_unlock(tty, irq_flags);
}

extern volatile uint64_t pit_ticks;

int tty_read(char *buf, uint64_t count) {
    if (!buf || count == 0)
        return 0;
    if (count > 0x7FFFFFFFULL)
        count = 0x7FFFFFFFULL;

    tty_t *tty = tty_get((uint8_t)multitasking_current_tty());
    uint64_t read = 0;
    uint64_t last_tick = pit_ticks;
    bool timer_started = false;
    uint64_t deadline = 0;
    linux_termios_t initial_termios;
    tty_get_termios(&initial_termios);
    uint8_t initial_vmin = initial_termios.c_cc[LINUX_VMIN];
    uint8_t initial_vtime = initial_termios.c_cc[LINUX_VTIME];
    uint64_t required = initial_vmin;
    if (required > count)
        required = count;
    if (initial_vmin == 0 && initial_vtime != 0) {
        timer_started = true;
        deadline = pit_ticks + (uint64_t)initial_vtime * 10;
    }

    for (;;) {
        if (tty_interrupt_pending())
            return read > 0 ? (int)read : -LINUX_EINTR;

        char c = 0;
        bool got = false, eof = false;

        uint64_t irq_flags = tty_lock(tty);
        bool canon = (tty->termios.c_lflag & LINUX_ICANON) != 0;
        uint8_t vmin = tty->termios.c_cc[LINUX_VMIN];
        uint8_t vtime = tty->termios.c_cc[LINUX_VTIME];
        if (rb_pop(&tty->cooked_rb, &c) == 0) {
            got = true;
        } else if (tty->eof) {
            tty->eof = false;
            eof = true;
        }
        tty_unlock(tty, irq_flags);

        if (got) {
            buf[read++] = c;
            if (read >= count)
                break;
            if (canon && c == '\n')
                break;
            if (!canon) {
                if (vmin == 0)
                    break;
                if (vtime != 0) {
                    timer_started = true;
                    deadline = pit_ticks + (uint64_t)vtime * 10;
                }
                if (read >= required)
                    break;
            }
            continue;
        }

        if (eof)
            break; /* returns 0 if nothing was read: EOF */
        if (!canon && vtime != 0 && timer_started && pit_ticks >= deadline)
            break;
        if (!canon && vmin == 0 && vtime == 0)
            break; /* VMIN=0, VTIME=0: poll */

        if (pit_ticks != last_tick) {
            last_tick = pit_ticks;
            multitasking_on_pit_tick(last_tick);
        }
        xhci_poll();
        asm volatile("hlt");
    }

    return (int)read;
}

bool tty_input_ready(void) {
    tty_t *tty = tty_get((uint8_t)multitasking_current_tty());
    uint64_t irq_flags = tty_lock(tty);
    bool ready = tty->eof || rb_size(&tty->cooked_rb) != 0;
    tty_unlock(tty, irq_flags);
    return ready;
}

uint64_t tty_write(const char *buf, uint64_t count) {
    if (!buf || count == 0)
        return 0;

    tty_t *tty = tty_get(tty_active_index());
    if (!tty->display)
        return 0;

    linux_termios_t termios;
    if (!tty_get_termios(&termios))
        return 0;

    char output[256];
    size_t used = 0;
    bool translate_newline = (termios.c_oflag & LINUX_OPOST) &&
                             (termios.c_oflag & LINUX_ONLCR);

    for (uint64_t i = 0; i < count; ++i) {
        char c = buf[i];
        if (translate_newline && c == '\n')
            output[used++] = '\r';
        output[used++] = c;

        if (used > sizeof(output) - 2) {
            flanterm_write(tty->display, output, used);
            used = 0;
        }
    }

    if (used != 0)
        flanterm_write(tty->display, output, used);
    return count;
}

void tty_flush_input(void) {
    tty_t *tty = tty_get((uint8_t)multitasking_current_tty());
    uint64_t irq_flags = tty_lock(tty);
    rb_clear(&tty->cooked_rb);
    tty->line_len = 0;
    tty->eof = false;
    tty->interrupt_exit_code = -1;
    tty_unlock(tty, irq_flags);
}

bool tty_interrupt_pending(void) {
    tty_t *tty = tty_get((uint8_t)multitasking_current_tty());
    return tty->interrupt_exit_code >= 0;
}

int tty_take_interrupt(void) {
    tty_t *tty = tty_get((uint8_t)multitasking_current_tty());
    if (tty->interrupt_exit_code < 0)
        return -1;

    uint64_t irq_flags = tty_lock(tty);
    int exit_code = tty->interrupt_exit_code;
    tty->interrupt_exit_code = -1;
    tty_unlock(tty, irq_flags);
    return exit_code;
}

void tty_clear_interrupt(void) {
    tty_t *tty = tty_get((uint8_t)multitasking_current_tty());
    uint64_t irq_flags = tty_lock(tty);
    tty->interrupt_exit_code = -1;
    tty_unlock(tty, irq_flags);
}

int tty_process_exit_code_for_key(int key) {
    tty_t *tty = tty_get(tty_active_index());
    int exit_code = -1;
    uint64_t irq_flags = tty_lock(tty);
    if ((tty->termios.c_lflag & LINUX_ISIG) &&
        tty->termios.c_cc[LINUX_VINTR] != 0 &&
        key == tty->termios.c_cc[LINUX_VINTR]) {
        exit_code = 130;
    }
    tty_unlock(tty, irq_flags);
    return exit_code;
}

bool tty_get_termios(linux_termios_t *termios) {
    if (!termios)
        return false;
    tty_t *tty = tty_get((uint8_t)multitasking_current_tty());
    uint64_t irq_flags = tty_lock(tty);
    *termios = tty->termios;
    tty_unlock(tty, irq_flags);
    return true;
}

bool tty_set_termios(const linux_termios_t *termios, bool flush_input) {
    if (!termios)
        return false;
    tty_t *tty = tty_get((uint8_t)multitasking_current_tty());
    uint64_t irq_flags = tty_lock(tty);
    tty->termios = *termios;
    if (flush_input) {
        rb_clear(&tty->cooked_rb);
        tty->line_len = 0;
        tty->eof = false;
    }
    tty_unlock(tty, irq_flags);
    return true;
}

bool tty_get_winsize(linux_winsize_t *winsize) {
    if (!winsize)
        return false;
    tty_t *tty = tty_get((uint8_t)multitasking_current_tty());
    winsize->ws_row = tty->display ? tty->display->rows : 25;
    winsize->ws_col = tty->display ? tty->display->cols : 80;
    winsize->ws_xpixel = 0;
    winsize->ws_ypixel = 0;
    return true;
}