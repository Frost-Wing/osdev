/**
 * @file graphics.h
 * @author Pradosh (pradoshgame@gmail.com) and (partially) GAMINGNOOB (https://github.com/GAMINGNOOBdev)
 * @brief Contains all the print functions.
 * @version 0.1
 * @date 2023-10-21
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#ifndef __GRAPHICS_H_
#define __GRAPHICS_H_ 1

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <basics.h>
#include <strings.h>

// ANSI color codes for text formatting
#define reset_color  "\033[37m"
#define red_color    "\x1b[91m"
#define yellow_color "\x1b[93m"
#define blue_color   "\x1b[36m"
#define green_color  "\x1b[32m"
#define orange_color "\x1b[38;5;208m"

extern string last_filename;
extern string last_filename; // for warn, info, err, done
extern string last_print_file;
extern string last_print_func;
extern int32 last_print_line;
extern bool enable_logging;

#define printf(fmt, ...) \
    printf_internal(__FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define printfnoln(fmt, ...) \
    printfnoln_internal(__FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)


/**
 * @brief Display a warning message.
 *
 * This function displays a warning message on the console with color formatting.
 *
 * @param message The warning message to be displayed.
 * @param file The file name where the warning occurred.
 */
void warn(cstring message, cstring  file);

/**
 * @brief Display an error message.
 *
 * This function displays an error message on the console with color formatting.
 *
 * @param message The error message to be displayed.
 * @param file The file name where the error occurred.
 */
void error(cstring message, cstring  file);

/**
 * @brief Display an informational message.
 *
 * This function displays an informational message on the console with color formatting.
 *
 * @param message The informational message to be displayed.
 * @param file The file name where the information is coming from.
 */
void info(cstring message, cstring  file);

/**
 * @brief Display a success message.
 *
 * This function displays a success message on the console with color formatting.
 *
 * @param message The success message to be displayed.
 * @param file The file name associated with the success.
 */
void done(cstring message, cstring  file);

/* Normal Hybrid printing functions ahead */

/**
 * @brief Prints a char, using print(&c);
 * 
 * @param c char to print
 */
void putc(char c);



/**
 * @brief Prints a value in binary format
 * 
 * @param value The value that will be printed
 */
void printbin(uint8_t value);

/**
 * @brief Prints with formatting supported.
 * 
 * @param file Filename where the function is called.
 * @param func The function name.
 * @param line The line.
 * @param format String.
 * @param ... 
 */
void printf_internal(cstring file, cstring func, int64 line, cstring format, ...);

/**
 * @brief Prints with formatting supported (does not add an new line).
 * 
 * @param file Filename where the function is called.
 * @param func The function name.
 * @param line The line.
 * @param format String.
 * @param ... 
 */
void printfnoln_internal(cstring file, cstring func, int64 line, cstring format, ...);

/**
 * @brief Core printf implementation used internally by both printf_internal and printfnoln_internal.
 * 
 * This function handles all formatted output processing, including support for
 * format specifiers such as %b, %x, %X, %u, %d, %s, and %c. It also interprets
 * escape characters like '\n', '\r', and '\t'. 
 * 
 * The newline flag controls whether a newline character ('\n') is printed
 * automatically at the end of the output.
 * 
 * @param file    The source file name of the caller (for logging context)
 * @param func    The function name of the caller (for logging context)
 * @param line    The source line number of the call (for logging context)
 * @param newline If true, appends a newline at the end of the formatted output
 * @param format  The printf-style format string
 * @param argp    The variable argument list (already started via va_start)
 */
void vprintf_internal(cstring file, cstring func, int64 line, bool newline, cstring format, va_list argp);

/**
 * @brief Formats a string into a buffer using a va_list.
 *
 * @param buf  destination buffer.
 * @param size buffer size in bytes.
 * @param fmt  format string.
 * @param args variable argument list.
 * @return number of characters written (excluding null terminator).
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/**
 * @brief Formats a string into a fixed-size buffer.
 *
 * @param buf  destination buffer.
 * @param size buffer size in bytes.
 * @param fmt  format string.
 * @return number of characters written (excluding null terminator).
 */
int snprintf(char *buf, size_t size, const char *fmt, ...);


/**
 * @brief Prints a char, using print(&c);
 * 
 * @param c char to print
 * @note Internally using Flanterm's putchar function
 */
void vputc(char c);

/**
 * @brief Print function for plain strings. (No Formatter)
 * 
 * @param msg The string.
 */
void print(cstring msg);

/**
 * @brief 
 * 
 * @param x 
 * @param y 
 * @param w 
 * @param h 
 * @param pixels 
 * @param color 
 */
void print_bitmap(int x, int y, int w, int h, bool* pixels, int32 color);

#endif