/**
 * @file logger.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief
 * @version 0.1
 * @date 2023-10-22
 *
 * @copyright Copyright (c) Pradosh 2023
 *
 */
#include <debugger.h>
#include <graphics.h>
#include <klog.h>
#include <opengl/glbackend.h>
#include <ringbuffer.h>
#include <stdarg.h>

#define LOG_MSG_MAX 256

extern struct flanterm_context *ft_ctx;
static stream_t printf_stream;

string last_filename = "unknown"; // for warn, info, err, done
string last_print_file = "unknown";
string last_print_func = "unknown";
uint32 last_print_line = 0;

bool enable_logging = true;

/* --- Depth tracking --- */
int log_depth = 0;

inline void __log_scope_exit(int *unused) {
    (void)unused;
    log_depth--;
}

/* --- Icons (ASCII, safe on any bitmap font) --- */
#define ICON_INFO  "[i]"
#define ICON_WARN  "[!]"
#define ICON_ERROR "[x]"
#define ICON_DONE  "[+]"

/* --- Tree pieces (CP437 box-drawing; swap to ASCII below if these don't render) --- */
// #define TREE_TRUNK  "\xB3  "      /* │   */
// #define TREE_END    "\xC0\xC4 "   /* └─  */


#define TREE_END    " └─ "
// #define TREE_BRANCH " ├─ "
#define TREE_TRUNK  " │  "

static void print_prefix(void) {
    for (int i = 0; i < log_depth - 1; i++) {
        printfnoln("%s", TREE_TRUNK);
        debug_print(TREE_TRUNK);
    }
    if (log_depth > 0) {
        printfnoln("%s", TREE_END);
        debug_print(TREE_END);
    }
}

static cstring strip_path(cstring file) {
    const char *slash = strrchr(file, '/');
    return slash ? slash + 1 : file;
}

static void log_tree(cstring icon, cstring color, cstring tag,
                      cstring file, cstring fmt, va_list args) {
    char message[LOG_MSG_MAX];
    vsnprintf(message, sizeof(message), fmt, args);

    file = strip_path(file);

    print_prefix();
    printf("%s%s %s" reset_color " " blue_color "%s" reset_color ": %s",
           color, icon, tag, file, message);

    debug_print(color); debug_print(icon); debug_print(" ");
    debug_print(tag); debug_print(reset_color " ");
    debug_print(blue_color); debug_print(file); debug_print(reset_color);
    debug_print(": "); debug_print(message); debug_print("\n");

    klog_printf("%s: %s (%s)", tag, message, file);
    last_filename = file;
}

void warn(cstring fmt, cstring file, ...) {
    va_list args;
    va_start(args, file);
    log_tree(ICON_WARN, yellow_color, "warn", file, fmt, args);
    va_end(args);
}

void error(cstring fmt, cstring file, ...) {
    va_list args;
    va_start(args, file);
    log_tree(ICON_ERROR, red_color, "error", file, fmt, args);
    va_end(args);
}

void info(cstring fmt, cstring file, ...) {
    va_list args;
    va_start(args, file);
    log_tree(ICON_INFO, blue_color, "info", file, fmt, args);
    va_end(args);
}

void done(cstring fmt, cstring file, ...) {
    va_list args;
    va_start(args, file);
    log_tree(ICON_DONE, green_color, "done", file, fmt, args);
    va_end(args);
}

void putc(char c) {
    if (c == '\b') {
        vputc('\b');
        vputc(' ');
    }

    vputc(c);
}

void vputc(char c) {
    stream_putc(printf_stream, c);
}

/**
 * @brief Prints a value in binary format
 *
 * @param value A pointer to the value that will be printed
 */
void printbin(uint8_t value) {
    static char binaryRepresentation[9];
    binaryRepresentation[8] = 0;

    for (int i = 0; i < 8; i++)
        binaryRepresentation[i] = (value & (0x80 >> i)) ? '1' : '0';

    print(binaryRepresentation);
}

static void printstr_fmt(const char *s, int width) {
    int len = 0;
    const char *p = s;

    while (*p++)
        len++;

    while (len < width) {
        putc(' ');
        width--;
    }

    print(s);
}

void vprintf_internal(stream_t stream, cstring file, cstring func, uint64 line, bool newline, cstring format, va_list argp) {
    if (enable_logging) {
        last_print_file = file;
        last_print_func = func;
        last_print_line = line;
    }

    printf_stream = stream;

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
                    format_number(buf,
                        va_arg(argp, int),
                        10, width, zero_pad, false);
                    print(buf);
                    break;
                }

                case 'u': {
                    char buf[64];
                    format_number(buf,
                        va_arg(argp, unsigned),
                        10, width, zero_pad, false);
                    print(buf);
                    break;
                }

                case 'x': {
                    char buf[64];
                    format_number(buf,
                        va_arg(argp, unsigned),
                        16, width, zero_pad, false);
                    print(buf);
                    break;
                }

                case 'X': {
                    char buf[64];
                    format_number(buf,
                        va_arg(argp, unsigned),
                        16, width, zero_pad, true);
                    print(buf);
                    break;
                }

                case 'b':
                    printbin((uint8_t)va_arg(argp, int));
                    break;

                case 's': {
                    const char *s = va_arg(argp, char *);
                    if (!s)
                        s = "(null)";
                    printstr_fmt(s, width);
                    break;
                }

                case 'c':
                    putc((char)va_arg(argp, int));
                    break;

                default:
                    putc('%');
                    putc(*format);
                    break;
            }
        } else {
            switch (*format) {
                default:
                    putc(*format);
                    break;
            }
        }
        format++;
    }

    if (newline)
        print("\n");
}

void printf_internal(cstring file, cstring func, uint64 line, cstring format, ...) {
    va_list argp;
    va_start(argp, format);
    vprintf_internal(STDOUT, file, func, line, true, format, argp);
    va_end(argp);
}

void printfnoln_internal(cstring file, cstring func, uint64 line, cstring format, ...) {
    va_list argp;
    va_start(argp, format);
    vprintf_internal(STDOUT, file, func, line, false, format, argp);
    va_end(argp);
}

void eprintf_internal(cstring file, cstring func, uint64 line, cstring format, ...) {
    va_list argp;
    va_start(argp, format);
    vprintf_internal(STDERR, file, func, line, true, format, argp);
    va_end(argp);
}

int format_number(
    char *out,
    long value,
    int base,
    int width,
    bool zero,
    bool upper) {
    char tmp[64];
    const char *digits = upper
                             ? "0123456789ABCDEF"
                             : "0123456789abcdef";

    int neg = 0;
    int i = 0;

    if (base == 10 && value < 0) {
        neg = 1;
        value = -value;
    }

    if (value == 0)
        tmp[i++] = '0';

    while (value > 0) {
        tmp[i++] = digits[value % base];
        value /= base;
    }

    if (neg)
        tmp[i++] = '-';

    int len = i;
    int pad = (width > len) ? (width - len) : 0;
    char padc = zero ? '0' : ' ';

    int pos = 0;
    while (pad--)
        out[pos++] = padc;

    while (i--)
        out[pos++] = tmp[i];

    out[pos] = '\0';
    return pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t outpos = 0;

// helper macro: append a single char safely
#define APPEND(ch)              \
    do {                        \
        if (outpos + 1 < size)  \
            buf[outpos] = (ch); \
        outpos++;               \
    } while (0)

// helper macro: append string safely
#define APPEND_STR(s)         \
    do {                      \
        const char *_p = (s); \
        while (*_p) {         \
            APPEND(*_p);      \
            _p++;             \
        }                     \
    } while (0)

    while (*fmt) {
        if (*fmt != '%') {
            APPEND(*fmt++);
            continue;
        }

        fmt++; // skip '%'

        bool zero = false;
        int width = 0;

        if (*fmt == '0') {
            zero = true;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        char numbuf[64];

        switch (*fmt) {
            case 'd':
                format_number(
                    numbuf,
                    va_arg(ap, int),
                    10, width, zero, false);
                APPEND_STR(numbuf);
                break;

            case 'u':
                format_number(
                    numbuf,
                    va_arg(ap, unsigned),
                    10, width, zero, false);
                APPEND_STR(numbuf);
                break;

            case 'x':
                format_number(
                    numbuf,
                    va_arg(ap, unsigned),
                    16, width, zero, false);
                APPEND_STR(numbuf);
                break;

            case 'X':
                format_number(
                    numbuf,
                    va_arg(ap, unsigned),
                    16, width, zero, true);
                APPEND_STR(numbuf);
                break;

            case 'c':
                APPEND((char)va_arg(ap, int));
                break;

            case 's': {
                const char *s = va_arg(ap, char *);
                if (!s)
                    s = "(null)";
                APPEND_STR(s);
                break;
            }

            case '%':
                APPEND('%');
                break;

            default:
                // unknown specifier → print literally
                APPEND('%');
                APPEND(*fmt);
                break;
        }

        fmt++;
    }

    // NUL terminate
    if (size > 0) {
        if (outpos >= size)
            buf[size - 1] = '\0';
        else
            buf[outpos] = '\0';
    }

    return outpos;
}

void print_bitmap(int x, int y, int w, int h, const bool *pixels, uint32 color) {
    int i, j, l;
    for (l = j = 0; l < h; l++) {
        for (i = 0; i < w; i++, j++) {
            if (pixels[j] == true)
                glWritePixel((uvec2){x + i, y + l}, color);
        }
    }
}

void print(cstring s) {
    if (!s)
        return;

    while (*s) {
        vputc(*s);
        s++;
    }
}

void kprint(cstring msg) {
    if (msg == null) {
        flanterm_write(ft_ctx, "null", 4);
        return;
    }
    flanterm_write(ft_ctx, msg, strlen(msg));
}