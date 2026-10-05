/**
 * @file logger.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief
 * @version 0.1
 * @date 2023-10-22
 *
 * @copyright Copyright (c) Pradosh 2023-2026
 *
 */
#include <debugger.h>
#include <flanterm/flanterm.h>
#include <graphics.h>
#include <klog.h>
#include <opengl/glbackend.h>
#include <ringbuffer.h>
#include <spinlock.h>
#include <stdarg.h>

#define LOG_MSG_MAX 256

extern struct flanterm_context *ft_ctx;
static stream_t printf_stream;
static spinlock_t console_lock = SPINLOCK_INITIALIZER;

static void print_unlocked(cstring s) {
    while (*s)
        vputc(*s++);
}

static void putc_unlocked(char c) {
    if (c == '\b') {
        vputc('\b');
        vputc(' ');
    }
    vputc(c);
}

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

static void log_tree(int level, cstring icon, cstring color, cstring tag,
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

    klog_printf_level(level, "%s: %s (%s)", tag, message, file);
    last_filename = file;
}

void warn(cstring fmt, cstring file, ...) {
    va_list args;
    va_start(args, file);
    log_tree(KLOG_WARNING, ICON_WARN, yellow_color, "warn", file, fmt, args);
    va_end(args);
}

void error(cstring fmt, cstring file, ...) {
    va_list args;
    va_start(args, file);
    log_tree(KLOG_ERR, ICON_ERROR, red_color, "error", file, fmt, args);
    va_end(args);
}

void info(cstring fmt, cstring file, ...) {
    va_list args;
    va_start(args, file);
    log_tree(KLOG_INFO, ICON_INFO, blue_color, "info", file, fmt, args);
    va_end(args);
}

void done(cstring fmt, cstring file, ...) {
    va_list args;
    va_start(args, file);
    log_tree(KLOG_INFO, ICON_DONE, green_color, "done", file, fmt, args);
    va_end(args);
}

void putc(char c) {
    spinlock_lock(&console_lock);
    putc_unlocked(c);
    spinlock_unlock(&console_lock);
}

void vputc(char c) {
    stream_putc(printf_stream, c);
}

void terminal_toggle_cursor(void) {
    spinlock_lock(&console_lock);
    if (ft_ctx) {
        ft_ctx->cursor_enabled = !ft_ctx->cursor_enabled;
        ft_ctx->double_buffer_flush(ft_ctx);
        __asm__ volatile("sfence" ::: "memory");
    }
    spinlock_unlock(&console_lock);
}

void terminal_switch_context(struct flanterm_context *context) {
    if (!context)
        return;

    spinlock_lock(&console_lock);
    ft_ctx = context;
    ft_ctx->full_refresh(ft_ctx);
    spinlock_unlock(&console_lock);
}

/**
 * @brief Prints a value in binary format
 *
 * @param value A pointer to the value that will be printed
 */
static void printbin_unlocked(uint8_t value) {
    static char binaryRepresentation[9];
    binaryRepresentation[8] = 0;

    for (int i = 0; i < 8; i++)
        binaryRepresentation[i] = (value & (0x80 >> i)) ? '1' : '0';

    print_unlocked(binaryRepresentation);
}

void printbin(uint8_t value) {
    spinlock_lock(&console_lock);
    printbin_unlocked(value);
    spinlock_unlock(&console_lock);
}

static void printstr_fmt(const char *s, int width) {
    int len = 0;
    const char *p = s;

    while (*p++)
        len++;

    while (len < width) {
        putc_unlocked(' ');
        width--;
    }

    print_unlocked(s);
}

void vprintf_internal(stream_t stream, cstring file, cstring func, uint64 line, bool newline, cstring format, va_list argp) {
    spinlock_lock(&console_lock);
    if (enable_logging) {
        last_print_file = file;
        last_print_func = func;
        last_print_line = line;
    }

    printf_stream = stream;

    while (*format != '\0') {
        if (*format != '%') {
            putc_unlocked(*format++);
            continue;
        }

        format++; /* skip '%' */
        if (*format == '\0') {
            putc_unlocked('%');
            break;
        }

        bool zero_pad = false;
        bool is32 = false;
        int width = 0;

        if (*format == '0') {
            zero_pad = true;
            format++;
        }

        while (*format >= '0' && *format <= '9') {
            width = (width * 10) + (*format - '0');
            format++;
        }

        /* length modifiers: l, ll, z, j, t are all 64-bit (the default); h = 32-bit */
        while (*format == 'l' || *format == 'z' || *format == 'j' || *format == 't')
            format++;
        if (*format == 'h') {
            is32 = true;
            format++;
        }

        char buf[FMT_BUF_SIZE];

        switch (*format) {
            case 'd':
            case 'i':
                format_number(buf, (uint64_t)FETCH_SIGNED(argp, is32),
                              true, 10, width, zero_pad, false);
                print_unlocked(buf);
                break;

            case 'u':
                format_number(buf, FETCH_UNSIGNED(argp, is32),
                              false, 10, width, zero_pad, false);
                print_unlocked(buf);
                break;

            case 'x':
                format_number(buf, FETCH_UNSIGNED(argp, is32),
                              false, 16, width, zero_pad, false);
                print_unlocked(buf);
                break;

            case 'X':
                format_number(buf, FETCH_UNSIGNED(argp, is32),
                              false, 16, width, zero_pad, true);
                print_unlocked(buf);
                break;

            case 'b':
                format_number(buf, FETCH_UNSIGNED(argp, is32),
                              false, 2, width, zero_pad, false);
                print_unlocked(buf);
                break;

            case 'p':
                print_unlocked("0x");
                format_number(buf, (uint64_t)va_arg(argp, void *),
                              false, 16, 16, true, false);
                print_unlocked(buf);
                break;

            case 's': {
                const char *s = va_arg(argp, char *);
                if (!s)
                    s = "(null)";
                printstr_fmt(s, width);
                break;
            }

            case 'c':
                putc_unlocked((char)va_arg(argp, int));
                break;

            case '%':
                putc_unlocked('%');
                break;

            default:
                putc_unlocked('%');
                putc_unlocked(*format);
                break;
        }
        format++;
    }

    if (newline)
        print_unlocked("\n");

    spinlock_unlock(&console_lock);
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
    uint64_t value,
    bool is_signed,
    int base,
    int width,
    bool zero,
    bool upper) {
    char tmp[65];
    const char *digits = upper
                             ? "0123456789ABCDEF"
                             : "0123456789abcdef";

    bool neg = false;
    int i = 0;

    if (width > FMT_WIDTH_MAX)
        width = FMT_WIDTH_MAX;

    if (is_signed && base == 10 && (int64_t)value < 0) {
        neg = true;
        value = (uint64_t)0 - value; /* safe even for INT64_MIN */
    }

    if (value == 0)
        tmp[i++] = '0';

    while (value > 0) {
        tmp[i++] = digits[value % (uint64_t)base];
        value /= (uint64_t)base;
    }

    int len = i + (neg ? 1 : 0);
    int pad = (width > len) ? (width - len) : 0;

    int pos = 0;

    if (neg && zero)
        out[pos++] = '-';

    while (pad--)
        out[pos++] = zero ? '0' : ' ';

    if (neg && !zero)
        out[pos++] = '-';

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
        if (*fmt == '\0') {
            APPEND('%');
            break;
        }

        bool zero = false;
        bool is32 = false;
        int width = 0;

        if (*fmt == '0') {
            zero = true;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        while (*fmt == 'l' || *fmt == 'z' || *fmt == 'j' || *fmt == 't')
            fmt++;
        if (*fmt == 'h') {
            is32 = true;
            fmt++;
        }

        char numbuf[FMT_BUF_SIZE];

        switch (*fmt) {
            case 'd':
            case 'i':
                format_number(numbuf, (uint64_t)FETCH_SIGNED(ap, is32),
                              true, 10, width, zero, false);
                APPEND_STR(numbuf);
                break;

            case 'u':
                format_number(numbuf, FETCH_UNSIGNED(ap, is32),
                              false, 10, width, zero, false);
                APPEND_STR(numbuf);
                break;

            case 'x':
                format_number(numbuf, FETCH_UNSIGNED(ap, is32),
                              false, 16, width, zero, false);
                APPEND_STR(numbuf);
                break;

            case 'X':
                format_number(numbuf, FETCH_UNSIGNED(ap, is32),
                              false, 16, width, zero, true);
                APPEND_STR(numbuf);
                break;

            case 'b':
                format_number(numbuf, FETCH_UNSIGNED(ap, is32),
                              false, 2, width, zero, false);
                APPEND_STR(numbuf);
                break;

            case 'p':
                APPEND('0');
                APPEND('x');
                format_number(numbuf, (uint64_t)va_arg(ap, void *),
                              false, 16, 16, true, false);
                APPEND_STR(numbuf);
                break;

            case 'c':
                APPEND((char)va_arg(ap, int));
                break;

            case 's': {
                const char *s = va_arg(ap, char *);
                if (!s)
                    s = "(null)";

                int len = 0;
                while (s[len])
                    len++;
                for (int p = len; p < width; p++)
                    APPEND(' ');

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

    return (int)outpos;

#undef APPEND
#undef APPEND_STR
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

    spinlock_lock(&console_lock);
    print_unlocked(s);
    spinlock_unlock(&console_lock);
}

void kprint(cstring msg) {
    spinlock_lock(&console_lock);
    if (msg == null) {
        flanterm_write(ft_ctx, "null", 4);
        spinlock_unlock(&console_lock);
        return;
    }
    flanterm_write(ft_ctx, msg, strlen(msg));
    spinlock_unlock(&console_lock);
}
