/**
 * @file syslog.c
 * @brief Implementation of the syscall logging subsystem (ring-buffer backed, PIT-timestamped).
 * @version 0.2
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 */
#include <ahci.h>
#include <syslog.h>
#include <pit.h>
#include <stdbool.h>

/* Reuses the integer-only number formatter already defined in logger.c,
 * so syslog needs no floating point of its own. */
extern int format_number(char *out, long value, int base, int width, bool zero, bool upper);

static uint8_t syslog_storage[SYSLOG_BUFFER_SIZE];

/* Defined here; logger.c declares this as extern and pushes into it from
 * vputc(), so console output and syslog_printf() share one buffer. */
ring_buffer_t syslog_rb;

bool is_syslog_ready = false;

void syslog_init(void)
{
    rb_init(&syslog_rb, syslog_storage, SYSLOG_BUFFER_SIZE, sizeof(char));
    is_syslog_ready = true;
}

void syslog_putc(char c)
{
    rb_push_overwrite(&syslog_rb, &c);
}

static void syslog_puts(cstring s)
{
    while (*s) {
        syslog_putc(*s);
        s++;
    }
}

/**
 * @brief Write "[secs.fraction] " straight off pit_ticks, integer math only.
 */
static void syslog_write_timestamp(void)
{
    uint64_t ticks = pit_ticks;

    uint64_t secs = ticks / SYSLOG_TICKS_PER_SEC;
    uint64_t rem  = ticks % SYSLOG_TICKS_PER_SEC;

    uint64_t scale = 1;
    for (int i = 0; i < SYSLOG_TS_FRAC_DIGITS; i++)
        scale *= 10;

    uint64_t frac = (rem * scale) / SYSLOG_TICKS_PER_SEC;

    char buf[32];

    syslog_putc('[');

    format_number(buf, (long)secs, 10, 0, false, false);
    syslog_puts(buf);

    syslog_putc('.');

    format_number(buf, (long)frac, 10, SYSLOG_TS_FRAC_DIGITS, true, false);
    syslog_puts(buf);

    syslog_putc(']');
    syslog_putc(' ');
}

void syslog_printf(cstring format, ...)
{
    if(!is_syslog_ready) return;
    
    va_list argp;
    va_start(argp, format);

    syslog_write_timestamp();

    while (*format != '\0') {
        if (*format == '%') {
            format++;

            bool zero_pad = false;
            int width = 0;

            if (*format == '0') {
                zero_pad = true;
                format++;
            }

            while (*format >= '0' && *format <= '9') {
                width = (width * 10) + (*format - '0');
                format++;
            }

            switch (*format) {
                case 'd': {
                    char buf[64];
                    format_number(buf, va_arg(argp, int), 10, width, zero_pad, false);
                    syslog_puts(buf);
                    break;
                }

                case 'u': {
                    char buf[64];
                    format_number(buf, va_arg(argp, unsigned), 10, width, zero_pad, false);
                    syslog_puts(buf);
                    break;
                }

                case 'x': {
                    char buf[64];
                    format_number(buf, va_arg(argp, unsigned), 16, width, zero_pad, false);
                    syslog_puts(buf);
                    break;
                }

                case 'X': {
                    char buf[64];
                    format_number(buf, va_arg(argp, unsigned), 16, width, zero_pad, true);
                    syslog_puts(buf);
                    break;
                }

                case 's': {
                    const char *s = va_arg(argp, char*);
                    if (!s) s = "(null)";
                    syslog_puts(s);
                    break;
                }

                case 'c':
                    syslog_putc((char)va_arg(argp, int));
                    break;

                case '%':
                    syslog_putc('%');
                    break;

                default:
                    syslog_putc('%');
                    syslog_putc(*format);
                    break;
            }
        }
        else {
            syslog_putc(*format);
        }

        format++;
    }

    syslog_putc('\n');

    va_end(argp);
}

size_t syslog_read(char* out, size_t max_len)
{
    return syslog_read_at(out, 0, max_len);
}

size_t syslog_read_at(char* out, size_t offset, size_t max_len)
{
    size_t total = rb_size(&syslog_rb);
    if (offset >= total)
        return 0;

    size_t avail = total - offset;
    size_t n = (avail < max_len) ? avail : max_len;

    size_t idx = (syslog_rb.tail + offset) % syslog_rb.capacity;
    for (size_t i = 0; i < n; i++) {
        out[i] = (char)syslog_rb.buffer[idx];
        idx = (idx + 1) % syslog_rb.capacity;
    }

    return n;
}

size_t syslog_size(void)
{
    return rb_size(&syslog_rb);
}

void syslog_clear(void)
{
    rb_clear(&syslog_rb);
}