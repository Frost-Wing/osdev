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
    char dummy[1];
    dummy[0] = c;
    print(dummy);
}

/**
 * @brief Prints decimal numbers
 * 
 * @param num the number to be printed
 */
void printdec(size_t num) {
    int i;
    char buf[21] = {0};

    if (!num) {
        putc('0');
        return;
    }

    for (i = 19; num; i--) {
        buf[i] = (num % 10) + 0x30;
        num = num / 10;
    }

    i++;
    print(buf + i);
}

/**
 * @brief Prints Hexadecimal number
 * 
 * @param hex the hexadecimal number to be printed.
 */
void printhex(int hex){
    char hex_str[16];
    itoa(hex, hex_str, 16, 16);
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

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            if (*format == 'x') {
                printhex(va_arg(argp, size_t));
            } else if (*format == 'd') {
                printdec(va_arg(argp, size_t));
            } else if (*format == 's') {
                print(va_arg(argp, char*));
            }
        } else {
            putc(*format);
        }
        format++;
    }

    putc('\n');
    va_end(argp);
}