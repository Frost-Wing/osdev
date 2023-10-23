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

// ANSI color codes for text formatting
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_RED "\x1b[91m"
#define ANSI_COLOR_YELLOW "\x1b[93m"
#define ANSI_COLOR_BLUE "\x1b[36m"
#define ANSI_COLOR_GREEN "\x1b[32m"

/**
 * @brief Display a warning message.
 *
 * This function displays a warning message on the console with color formatting.
 *
 * @param message The warning message to be displayed.
 * @param file The file name where the warning occurred.
 */
void warn(const char *message, const char* file) {
    const char *warn_message = ANSI_COLOR_YELLOW "[Warning] → " ANSI_COLOR_RESET;
    print(warn_message);
    print(message);
    print(" at " ANSI_COLOR_BLUE);
    print(file);
    print(ANSI_COLOR_RESET "\n");
}

/**
 * @brief Display an error message.
 *
 * This function displays an error message on the console with color formatting.
 *
 * @param message The error message to be displayed.
 * @param file The file name where the error occurred.
 */
void error(const char *message, const char* file) {
    const char *err_message = ANSI_COLOR_RED "[Error] → " ANSI_COLOR_RESET;
    print(err_message);
    print(message);
    print(" at " ANSI_COLOR_BLUE);
    print(file);
    print(ANSI_COLOR_RESET "\n");
}

/**
 * @brief Display an informational message.
 *
 * This function displays an informational message on the console with color formatting.
 *
 * @param message The informational message to be displayed.
 * @param file The file name where the information is coming from.
 */
void info(const char *message, const char* file) {
    const char *info_message = ANSI_COLOR_BLUE "[Info] → " ANSI_COLOR_RESET;
    print(info_message);
    print(message);
    print(" at " ANSI_COLOR_BLUE);
    print(file);
    print(ANSI_COLOR_RESET "\n");
}

/**
 * @brief Display a success message.
 *
 * This function displays a success message on the console with color formatting.
 *
 * @param message The success message to be displayed.
 * @param file The file name associated with the success.
 */
void done(const char *message, const char* file) {
    const char *done_message = ANSI_COLOR_GREEN "[Success] → " ANSI_COLOR_RESET;
    print(done_message);
    print(message);
    print(" at " ANSI_COLOR_BLUE);
    print(file);
    print(ANSI_COLOR_RESET "\n");
}

/**
 * @brief Prints a char, using print(&c);
 * 
 * @param c char to print
 */
void putc(char c) {
    print(&c);
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
void printf(const char *format, ...) {
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