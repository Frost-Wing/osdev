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

// ANSI color codes for text formatting
#define reset_color  "\x1b[0m"
#define red_color    "\x1b[91m"
#define yellow_color "\x1b[93m"
#define blue_color   "\x1b[36m"
#define green_color  "\x1b[32m"

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
 * @brief Prints decimal numbers
 * 
 * @param num the number to be printed
 */
void printdec(size_t num);

/**
 * @brief Prints Hexadecimal number
 * 
 * @param hex the hexadecimal number to be printed.
 */
void printhex(int hex);

/**
 * @brief More uniform print function.
 * Supports any number of arguments (va_list)
 * 
 * @param format 
 * @param ... 
 */
void printf(cstring format, ...);

#endif