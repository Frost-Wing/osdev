/**
 * @file klog.c
 * @brief Implementation of the kernel logging subsystem (ring-buffer backed, PIT-timestamped).
 * @version 0.2
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 */
#include <ahci.h>
#include <klog.h>
#include <pit.h>
#include <stdbool.h>

/* Reuses the integer-only number formatter already defined in logger.c,
 * so klog needs no floating point of its own. */
extern int format_number(char *out, long value, int base, int width, bool zero, bool upper);

static uint8_t klog_storage[KLOG_BUFFER_SIZE];

/* Defined here; logger.c declares this as extern and pushes into it from
 * vputc(), so console output and klog_printf() share one buffer. */
ring_buffer_t klog_rb;

extern uint64_t pit_ticks;

bool is_klog_ready = false;

void klog_init(void)
{
    rb_init(&klog_rb, klog_storage, KLOG_BUFFER_SIZE, sizeof(char));
    is_klog_ready = true;
}

void klog_putc(char c)
{
    rb_push_overwrite(&klog_rb, &c);
}

static void klog_puts(cstring s)
{
    while (*s) {
        klog_putc(*s);
        s++;
    }
}

/**
 * @brief Write "[secs.fraction] " straight off pit_ticks, integer math only.
 */
static void klog_write_timestamp(void)
{
    uint64_t ticks = pit_ticks;

    uint64_t secs = ticks / KLOG_TICKS_PER_SEC;
    uint64_t rem  = ticks % KLOG_TICKS_PER_SEC;

    uint64_t scale = 1;
    for (int i = 0; i < KLOG_TS_FRAC_DIGITS; i++)
        scale *= 10;

    uint64_t frac = (rem * scale) / KLOG_TICKS_PER_SEC;

    char buf[32];

    klog_putc('[');

    format_number(buf, (long)secs, 10, 0, false, false);
    klog_puts(buf);

    klog_putc('.');

    format_number(buf, (long)frac, 10, KLOG_TS_FRAC_DIGITS, true, false);
    klog_puts(buf);

    klog_putc(']');
    klog_putc(' ');
}

void klog_printf(cstring format, ...)
{
    if(!is_klog_ready) return;
    
    va_list argp;
    va_start(argp, format);

    klog_write_timestamp();

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
                    klog_puts(buf);
                    break;
                }

                case 'u': {
                    char buf[64];
                    format_number(buf, va_arg(argp, unsigned), 10, width, zero_pad, false);
                    klog_puts(buf);
                    break;
                }

                case 'x': {
                    char buf[64];
                    format_number(buf, va_arg(argp, unsigned), 16, width, zero_pad, false);
                    klog_puts(buf);
                    break;
                }

                case 'X': {
                    char buf[64];
                    format_number(buf, va_arg(argp, unsigned), 16, width, zero_pad, true);
                    klog_puts(buf);
                    break;
                }

                case 's': {
                    const char *s = va_arg(argp, char*);
                    if (!s) s = "(null)";
                    klog_puts(s);
                    break;
                }

                case 'c':
                    klog_putc((char)va_arg(argp, int));
                    break;

                case '%':
                    klog_putc('%');
                    break;

                default:
                    klog_putc('%');
                    klog_putc(*format);
                    break;
            }
        }
        else {
            klog_putc(*format);
        }

        format++;
    }

    klog_putc('\n');

    va_end(argp);
}

size_t klog_read(char* out, size_t max_len)
{
    return klog_read_at(out, 0, max_len);
}

size_t klog_read_at(char* out, size_t offset, size_t max_len)
{
    size_t total = rb_size(&klog_rb);
    if (offset >= total)
        return 0;

    size_t avail = total - offset;
    size_t n = (avail < max_len) ? avail : max_len;

    size_t idx = (klog_rb.tail + offset) % klog_rb.capacity;
    for (size_t i = 0; i < n; i++) {
        out[i] = (char)klog_rb.buffer[idx];
        idx = (idx + 1) % klog_rb.capacity;
    }

    return n;
}

size_t klog_size(void)
{
    return rb_size(&klog_rb);
}

void klog_clear(void)
{
    rb_clear(&klog_rb);
}