/**
 * @file termios.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef SYS_TERMIOS_H
#define SYS_TERMIOS_H

#include <stdint.h>

/**
 * @brief Terminal window size information.
 *
 * Describes the dimensions of a terminal in characters and pixels.
 * Commonly used with ioctl calls
 */
typedef struct {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} linux_winsize_t;

/**
 * @brief Terminal I/O configuration structure.
 *
 * Controls behavior of terminal devices such as input processing,
 * output processing, line discipline, and local modes.
 * Used with terminal-related syscalls (e.g., tcgetattr, tcsetattr).
 */
typedef struct {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t c_line;
    uint8_t c_cc[19];
} linux_termios_t;

#endif