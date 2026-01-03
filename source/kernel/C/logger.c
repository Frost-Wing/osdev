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

static const char hex_digits[] = "0123456789abcdef";
static const char caps_hex_digits[] = "0123456789ABCDEF";
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

/**
 * @brief Prints a char, using print(&c);
 * 
 * @param c char to print
 * @note Internally using Flanterm's putchar function
 */
void vputc(char c) {
    stream_putc(printf_stream, c);
}

void printdec_fmt(int num, int width, bool zero_pad)
{
    char buf[21];
    int i = 20;
    bool negative = false;

    buf[i] = '\0';

    if (num == 0) {
        buf[--i] = '0';
    } else {
        if (num < 0) {
            negative = true;
            num = -num;
        }

        while (num > 0) {
            buf[--i] = (num % 10) + '0';
            num /= 10;
        }
    }

    int len = 20 - i;

    if (negative)
        len++;

    while (len < width) {
        buf[--i] = zero_pad ? '0' : ' ';
        len++;
    }

    if (negative)
        buf[--i] = '-';

    print(&buf[i]);
}

void printdec_unsigned_fmt(unsigned int num, int width, bool zero_pad)
{
    char buf[21];
    int i = 20;

    buf[i] = '\0';

    if (num == 0) {
        buf[--i] = '0';
    } else {
        while (num > 0) {
            buf[--i] = (num % 10) + '0';
            num /= 10;
        }
    }

    int len = 20 - i;

    while (len < width) {
        buf[--i] = zero_pad ? '0' : ' ';
        len++;
    }

    print(&buf[i]);
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
        binaryRepresentation[i] = (value & 0x80 >> i) ? '1' : '0';
    
    print(binaryRepresentation);
}

void printhex_fmt(size_t value, int width, bool uppercase) {
    char buf[16];
    int len = 0;

    const char* digits = uppercase
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    do {
        buf[len++] = digits[value & 0xF];
        value >>= 4;
    } while (value && len < 16);

    while (len < width)
        buf[len++] = '0';

    for (int i = len - 1; i >= 0; i--)
        putc(buf[i]);
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
                case 'd':
                    printdec_fmt(va_arg(argp, int), width, zero_pad);
                    break;

                case 'u':
                    printdec_unsigned_fmt(va_arg(argp, unsigned int), width, zero_pad);
                    break;

                case 'x':
                    printhex_fmt(va_arg(argp, size_t),
                                zero_pad ? width : 0,
                                false);
                    break;

                case 'X':
                    printhex_fmt(va_arg(argp, size_t),
                                zero_pad ? width : 0,
                                true);
                    break;

                case 'b':
                    printbin((uint8_t)va_arg(argp, int));
                    break;

                case 's':
                    printstr_fmt(va_arg(argp, char*), width);
                    break;

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
                case '\n': print("\n"); break;
                case '\r': print("\r"); break;
                case '\t': print("\t"); break;
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

static char *format_number(long value, int base, int width, bool zero, bool upper)
{
    char tmp[64];
    const char *digits = upper ?
        "0123456789ABCDEF" :
        "0123456789abcdef";

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
    char *out = kmalloc(len + pad + 1);
    int pos = 0;

    while (pad--)
        out[pos++] = padc;

    while (i--)
        out[pos++] = tmp[i];

    out[pos] = '\0';
    return out;
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
    char *out = kmalloc(4096);   // fixed large scratch buffer
    size_t outpos = 0;

    while (*fmt) {
        if (*fmt != '%') {
            out[outpos++] = *fmt++;
            continue;
        }

        fmt++;

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

        char *tmp = NULL;

        switch (*fmt) {
            case 'd':
                tmp = format_number(va_arg(ap, int), 10, width, zero, false);
                break;
            case 'u':
                tmp = format_number(va_arg(ap, unsigned), 10, width, zero, false);
                break;
            case 'x':
                tmp = format_number(va_arg(ap, unsigned), 16, width, zero, false);
                break;
            case 'X':
                tmp = format_number(va_arg(ap, unsigned), 16, width, zero, true);
                break;
            case 'c':
                out[outpos++] = (char)va_arg(ap, int);
                break;
            case 's': {
                const char *s = va_arg(ap, char *);
                if (!s) s = "(null)";
                while (*s)
                    out[outpos++] = *s++;
                break;
            }
            case '%':
                out[outpos++] = '%';
                break;
        }

        if (tmp) {
            char *p = tmp;
            while (*p)
                out[outpos++] = *p++;
            kfree(tmp);
        }

        fmt++;
    }

    out[outpos] = '\0';

    size_t copy = (size > 0 && outpos >= size) ? size - 1 : outpos;
    for (size_t i = 0; i < copy; i++)
        buf[i] = out[i];

    if (size)
        buf[copy] = '\0';

    kfree(out);
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
