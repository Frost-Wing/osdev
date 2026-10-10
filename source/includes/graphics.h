/**
 * @file graphics.h
 * @author Pradosh (pradoshgame@gmail.com) and (partially) GAMINGNOOB (https://github.com/GAMINGNOOBdev)
 * @brief Contains all the print functions.
 * @version 0.1
 * @date 2023-10-21
 *
 * @copyright Copyright (c) Pradosh 2023-2026
 *
 */
#ifndef __GRAPHICS_H_
#define __GRAPHICS_H_

#include <basics.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stream.h>
#include <strings.h>

#define FMT_WIDTH_MAX 64
#define FMT_BUF_SIZE  80

/* Default is 64-bit for everything. Use %h... if you really want 32-bit. */
#define FETCH_SIGNED(ap, is32) \
    ((is32) ? (int64_t)va_arg(ap, int) : (int64_t)va_arg(ap, int64_t))
#define FETCH_UNSIGNED(ap, is32) \
    ((is32) ? (uint64_t)va_arg(ap, unsigned int) : (uint64_t)va_arg(ap, uint64_t))

// ANSI color codes for text formatting
#define reset_color  "\x1b[38;2;248;248;242m" // Dracula foreground — warm off-white
#define red_color    "\x1b[38;2;255;85;85m"   // Dracula red
#define yellow_color "\x1b[38;2;241;250;140m" // Dracula yellow — pale lemon (very signature)
#define blue_color   "\x1b[38;2;189;147;249m" // Dracula purple standing in as your "blue" slot
#define green_color  "\x1b[38;2;80;250;123m"  // Dracula green — vivid mint
#define orange_color "\x1b[38;2;255;184;108m" // Dracula orange

// #define reset_color  "\x1b[38;5;248m"
// #define red_color    "\x1b[38;5;167m"
// #define yellow_color "\x1b[38;5;179m"
// #define blue_color   "\x1b[38;5;75m"
// #define green_color  "\x1b[38;5;71m"
// #define orange_color "\x1b[38;5;172m"

extern string last_filename; // for warn, info, err, done
extern string last_print_file;
extern string last_print_func;
extern uint32 last_print_line;
extern bool enable_logging;

extern int log_depth;
extern bool log_tree_enabled;
void log_tree_set(bool enabled);
bool log_tree_is_enabled(void);

/**
 * @brief Decrement the logging depth when a scoped logging function returns.
 * @param unused Cleanup attribute parameter.
 */
extern inline void __log_scope_exit(int *unused);
/* Place at the top of any function you want tracked in the tree.
 * Auto-decrements log_depth when the function returns, however it returns. */
#define LOG_SCOPE() \
    int __log_guard__ __attribute__((cleanup(__log_scope_exit))) = (log_depth++, 0)

#define LOG_BLOCK \
    for (int __log_guard__ __attribute__((cleanup(__log_scope_exit))) = (log_depth++, 0); \
         !__log_guard__; __log_guard__ = 1)

#define printf(fmt, ...) \
    printf_internal(__FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define printfnoln(fmt, ...) \
    printfnoln_internal(__FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define eprintf(fmt, ...) \
    eprintf_internal(__FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Display a warning message.
 *
 * This function displays a warning message on the console with color formatting.
 *
 * @param message The warning message to be displayed.
 * @param file The file name where the warning occurred.
 */
void warn(cstring message, cstring file, ...);

/**
 * @brief Display an error message.
 *
 * This function displays an error message on the console with color formatting.
 *
 * @param message The error message to be displayed.
 * @param file The file name where the error occurred.
 */
void error(cstring message, cstring file, ...);

/**
 * @brief Display an informational message.
 *
 * This function displays an informational message on the console with color formatting.
 *
 * @param message The informational message to be displayed.
 * @param file The file name where the information is coming from.
 */
void info(cstring message, cstring file, ...);

/**
 * @brief Display a success message.
 *
 * This function displays a success message on the console with color formatting.
 *
 * @param message The success message to be displayed.
 * @param file The file name associated with the success.
 */
void done(cstring message, cstring file, ...);

/* Normal Hybrid printing functions ahead */

/**
 * @brief Prints a character, converting backspace into a backspace-space-backspace sequence.
 *
 * @param c char to print
 */
void putc(char c);

/**
 * @brief Prints a char though the standard streams.
 *
 * @param c The charecter to be printed.
 */
void vputc(char c);

/**
 * @brief Prints a value in binary format
 *
 * @param value The value that will be printed
 */
void printbin(uint8_t value);

/**
* @brief Format an integer into a buffer using the requested base and width.
*
* @param out Destination buffer.
* @param value Integer value to format.
* @param is_signed Whether the value should be treated as signed.
* @param base Numeric base.
* @param width Minimum output width.
* @param zero Whether to pad with zeroes.
* @param upper Whether to use uppercase digits.
* @return Number of characters written, or a negative value on error.
*/
int format_number(char *out, uint64_t value, bool is_signed,
                  int base, int width, bool zero, bool upper);

/**
 * @brief Prints with formatting supported.
 *
 * @param file Filename where the function is called.
 * @param func The function name.
 * @param line The line.
 * @param format String.
 * @param ...
 */
void printf_internal(cstring file, cstring func, uint64 line, cstring format, ...);

/**
 * @brief Prints with formatting supported (does not add an new line).
 *
 * @param file Filename where the function is called.
 * @param func The function name.
 * @param line The line.
 * @param format String.
 * @param ...
 */
void printfnoln_internal(cstring file, cstring func, uint64 line, cstring format, ...);

/**
 * @brief Print an error message with source location and formatting.
 *
 * @param file Source file name.
 * @param func Calling function name.
 * @param line Source line number.
 * @param format printf-style format string.
 * @param ... Values matching the format string.
 */
void eprintf_internal(cstring file, cstring func, uint64 line, cstring format, ...);

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
 * @param stream  The unix like STDOUT, STDERR.
 * @param file    The source file name of the caller (for logging context)
 * @param func    The function name of the caller (for logging context)
 * @param line    The source line number of the call (for logging context)
 * @param newline If true, appends a newline at the end of the formatted output
 * @param format  The printf-style format string
 * @param argp    The variable argument list (already started via va_start)
 */
void vprintf_internal(stream_t stream, cstring file, cstring func, uint64 line, bool newline, cstring format, va_list argp);

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
 * @brief RAW kernel print function for plain strings. (No Formatter & No Streams)
 *
 * @param msg The string.
 */
void kprint(cstring msg);

/**
 * @brief Plain print function, goes through the stream.
 *
 * @param s Normal string to be displayed
 */
void print(cstring s);

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
void print_bitmap(int x, int y, int w, int h, const bool *pixels, uint32 color);

/**
 * @brief Toggle and redraw the terminal cursor while holding the console lock.
 *
 * This may be called by an AP while the BSP is halted waiting for input.
 */
void terminal_toggle_cursor(void);

/**
 * @brief Atomically select and redraw the framebuffer terminal used for console I/O.
 * @param context Terminal context to activate.
 */
void terminal_switch_context(struct flanterm_context *context);

#endif
