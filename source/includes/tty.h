#ifndef TTY_H
#define TTY_H

#include <basics.h>
#include <stdint.h>

#define TTY_LINE_MAX     256
#define TTY_COOKED_MAX   1024

void tty_init(void);
void tty_input_char(char c);
int tty_read(char* buf, uint64_t count);

#endif
