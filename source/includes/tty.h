#ifndef TTY_H
#define TTY_H

#include <basics.h>
#include <stdint.h>
#include <stdbool.h>

struct flanterm_context;

#define TTY_LINE_MAX 256
#define TTY_COOKED_MAX 1024
#define TTY_COUNT 7
/* Ask the task layer to inherit its parent's terminal. */
#define TTY_INDEX_CURRENT UINT8_MAX

void tty_init(void);
/* Attach seven virtual terminals to an already initialized framebuffer console. */
bool tty_init_terminals(struct flanterm_context *default_terminal,
                        uint32_t *framebuffer, size_t width, size_t height,
                        size_t pitch);
void tty_input_char(char c);
int tty_read(char *buf, uint64_t count);
void tty_flush_input(void);
bool tty_switch(uint8_t index);
uint8_t tty_active_index(void);
void tty_input_key(int key);

#endif
