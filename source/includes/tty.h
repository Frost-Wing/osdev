#ifndef TTY_H
#define TTY_H

#include <basics.h>
#include <sys/termios.h>
#include <stdint.h>
#include <stdbool.h>

struct flanterm_context;

#define TTY_LINE_MAX 256
#define TTY_COOKED_MAX 1024
#define TTY_COUNT 1
/* Ask the task layer to inherit its parent's terminal. */
#define TTY_INDEX_CURRENT UINT8_MAX

void tty_init(void);
/*
 * Attach the single kernel terminal (tty0) to the already initialized
 * framebuffer console. The extra parameters are kept so existing callers
 * don't change; no extra terminals are created, so nothing is allocated here.
 */
bool tty_init_terminals(struct flanterm_context *default_terminal,
                        uint32_t *framebuffer, size_t width, size_t height,
                        size_t pitch, const void *ssfn_font,
                        size_t ssfn_font_size);
void tty_input_char(char c);
int tty_read(char *buf, uint64_t count);
void tty_flush_input(void);
bool tty_interrupt_pending(void);
int tty_take_interrupt(void);
void tty_clear_interrupt(void);
int tty_process_exit_code_for_key(int key);
bool tty_get_termios(linux_termios_t *termios);
bool tty_set_termios(const linux_termios_t *termios, bool flush_input);
bool tty_get_winsize(linux_winsize_t *winsize);
bool tty_switch(uint8_t index);
uint8_t tty_active_index(void);
void tty_input_key(int key);

#endif