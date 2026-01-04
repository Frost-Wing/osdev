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
#include <graphics.h>
#include <opengl/glbackend.h>

extern struct flanterm_context* ft_ctx;
static stream_t printf_stream;

string last_filename = "unknown"; // for warn, info, err, done
string last_print_file = "unknown";
string last_print_func = "unknown";
int32 last_print_line = 0;

bool enable_logging = true;

/**
 * @brief Display a warning message.
 *
 * This function displays a warning message on the console with color formatting.
 *
 * @param message The warning message to be displayed.
 * @param file The file name where the warning occurred.
 */
void warn(cstring message, cstring file) {
    cstring warn_message = yellow_color "[***] → " reset_color;
    printf("%s%s at " blue_color "%s" reset_color, warn_message, message, file);

    debug_print(warn_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");

    last_filename = file;
}

/**
 * @brief Display an error message.
 *
 * This function displays an error message on the console with color formatting.
 *
 * @param message The error message to be displayed.
 * @param file The file name where the error occurred.
 */
void error(cstring message, cstring file) {
    cstring err_message = red_color "[***] → " reset_color;
    eprintf("%s%s at " blue_color "%s" reset_color, err_message, message, file);

    debug_print(err_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");

    last_filename = file;
}

/**
 * @brief Display an informational message.
 *
 * This function displays an informational message on the console with color formatting.
 *
 * @param message The informational message to be displayed.
 * @param file The file name where the information is coming from.
 */
void info(cstring message, cstring file) {
    cstring info_message = blue_color "[***] → " reset_color;
    printf("%s%s at " blue_color "%s" reset_color, info_message, message, file);

    debug_print(info_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");

    last_filename = file;
}

/**
 * @brief Display a success message.
 *
 * This function displays a success message on the console with color formatting.
 *
 * @param message The success message to be displayed.
 * @param file The file name associated with the success.
 */
void done(cstring message, cstring file) {
    cstring done_message = green_color "[***] → " reset_color;
    printf("%s%s at " blue_color "%s" reset_color, done_message, message, file);

    debug_print(done_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");

    last_filename = file;
}

void putc(char c){
    if (c == '\b')
    {
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
void printbin(uint8_t value)
{
    static char binaryRepresentation[9];
    binaryRepresentation[8] = 0;

    for (int i = 0; i < 8; i++)
        binaryRepresentation[i] = (value & (0x80 >> i)) ? '1' : '0';
    
    print(binaryRepresentation);
}

static void printstr_fmt(const char* s, int width)
{
    int len = 0;
    const char* p = s;

    while (*p++) len++;

    while (len < width) {
        putc(' ');
        width--;
    }

    print(s);
}

void vprintf_internal(stream_t stream, cstring file, cstring func, int64 line, bool newline, cstring format, va_list argp) {
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
                    const char *s = va_arg(argp, char*);
                    if (!s) s = "(null)";
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
        }
        else {
            switch (*format) {
                default: putc(*format); break;
            }
        }
        format++;
    }

    if (newline)
        print("\n");
}

void printf_internal(cstring file, cstring func, int64 line, cstring format, ...) {
    va_list argp;
    va_start(argp, format);
    vprintf_internal(STDOUT, file, func, line, true, format, argp);
    va_end(argp);
}

void printfnoln_internal(cstring file, cstring func, int64 line, cstring format, ...) {
    va_list argp;
    va_start(argp, format);
    vprintf_internal(STDOUT, file, func, line, false, format, argp);
    va_end(argp);
}

void eprintf_internal(cstring file, cstring func, int64 line, cstring format, ...) {
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
    bool upper
) {
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

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    size_t outpos = 0;

    // helper macro: append a single char safely
    #define APPEND(ch) \
        do { \
            if (outpos + 1 < size) \
                buf[outpos] = (ch); \
            outpos++; \
        } while (0)

    // helper macro: append string safely
    #define APPEND_STR(s) \
        do { \
            const char *_p = (s); \
            while (*_p) { \
                APPEND(*_p); \
                _p++; \
            } \
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
                if (!s) s = "(null)";
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

void print_bitmap(int x, int y, int w, int h, bool* pixels, int32 color) {
    int i, j, l;
    for (l = j = 0; l < h; l++) {
        for (i = 0; i < w; i++, j++) {
            if(pixels[j] == true) glWritePixel((uvec2){x + i, y + l}, color);
        }
    }
}

void print(cstring s)
{
    if (!s) return;

    while (*s)
    {
        vputc(*s);
        s++;
    }
}

void kprint(cstring msg) {
    if(msg == null){
        flanterm_write(ft_ctx, "null", 4);
        return;
    }
    flanterm_write(ft_ctx, msg, strlen(msg));
}