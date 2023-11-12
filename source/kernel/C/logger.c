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

/**
 * @brief Display a warning message.
 *
 * This function displays a warning message on the console with color formatting.
 *
 * @param message The warning message to be displayed.
 * @param file The file name where the warning occurred.
 */
void warn(cstring message, cstring file) {
    cstring warn_message = yellow_color "[Warning] → " reset_color;
    print(warn_message);
    print(message);
    print(" at " blue_color);
    print(file);
    print(reset_color "\n");

    debug_print(warn_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");

    serial_print(message);
    serial_print(" at ");
    serial_print(file);
    serial_print("\n");
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
    cstring err_message = red_color "[Error] → " reset_color;
    print(err_message);
    print(message);
    print(" at " blue_color);
    print(file);
    print(reset_color "\n");

    debug_print(err_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");

    serial_print(message);
    serial_print(" at ");
    serial_print(file);
    serial_print("\n");
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
    cstring info_message = blue_color "[Info] → " reset_color;
    print(info_message);
    print(message);
    print(" at " blue_color);
    print(file);
    print(reset_color "\n");

    debug_print(info_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");

    serial_print(message);
    serial_print(" at ");
    serial_print(file);
    serial_print("\n");
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
    cstring done_message = green_color "[Success] → " reset_color;
    print(done_message);
    print(message);
    print(" at " blue_color);
    print(file);
    print(reset_color "\n");

    debug_print(done_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");

    serial_print(message);
    serial_print(" at ");
    serial_print(file);
    serial_print("\n");
}

/**
 * @brief Prints a char, using print(&c);
 * 
 * @param c char to print
 */
void putc(char c) {
    char _c[1];
    _c[0] = c;
    print(_c);
}

/**
 * @brief Prints decimal numbers
 * 
 * @param num the number to be printed
 */
void printdec(size_t num) {
    if (num == 0) {
        putc('0');
        return;
    }

    char buf[21]; // Sufficient for a 64-bit number
    int i = sizeof(buf) - 1;
    buf[i] = '\0';

    while (num > 0) {
        buf[--i] = (num % 10) + '0';
        num /= 10;
    }

    print(&buf[i]);
}

/**
 * @brief Prints Hexadecimal number
 * 
 * @param hex the hexadecimal number to be printed.
 */
void printhex(int hex) {
    char hex_str[9]; // Assuming 32-bit integer
    char hex_digits[] = "0123456789ABCDEF";

    for (int i = 0; i < 8; i++) {
        int nibble = (hex >> (28 - i * 4)) & 0xF;
        hex_str[i] = hex_digits[nibble];
    }
    hex_str[8] = '\0';

    print(hex_str);
}
/**
 * @brief More uniform print function.
 * Supports any number of arguments (va_list)
 * 
 * @param format 
 * @param ... 
 */
void printf(cstring format, ...) {
    va_list argp;
    va_start(argp, format);

    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'x':
                    printhex(va_arg(argp, size_t));
                    break;
                case 'd':
                    printdec(va_arg(argp, int));
                    break;
                case 's':
                    print(va_arg(argp, char*));
                    break;
                default:
                    putc('%');
                    putc(*format);
                    break;
            }
        } else {
            putc(*format);
        }
        format++;
    }
    print("\n");
    va_end(argp);
}