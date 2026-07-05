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

/* c_iflag */
#define LINUX_IGNBRK 0x00000001
#define LINUX_BRKINT 0x00000002
#define LINUX_IGNPAR 0x00000004
#define LINUX_PARMRK 0x00000008
#define LINUX_INPCK 0x00000010
#define LINUX_ISTRIP 0x00000020
#define LINUX_INLCR 0x00000040
#define LINUX_IGNCR 0x00000080
#define LINUX_ICRNL 0x00000100
#define LINUX_IXON 0x00000400
#define LINUX_IXOFF 0x00001000
#define LINUX_IXANY 0x00000800

/* c_oflag */
#define LINUX_OPOST 0x00000001
#define LINUX_ONLCR 0x00000004

/* c_cflag */
#define LINUX_CS5 0x00000000
#define LINUX_CS6 0x00000010
#define LINUX_CS7 0x00000020
#define LINUX_CS8 0x00000030
#define LINUX_CSIZE 0x00000030
#define LINUX_CREAD 0x00000080

/* c_lflag */
#define LINUX_ISIG 0x00000001
#define LINUX_ICANON 0x00000002
#define LINUX_ECHO 0x00000008
#define LINUX_ECHOE 0x00000010
#define LINUX_ECHOK 0x00000020
#define LINUX_ECHONL 0x00000040
#define LINUX_NOFLSH 0x00000080
#define LINUX_IEXTEN 0x00008000

#define LINUX_VINTR 0
#define LINUX_VQUIT 1
#define LINUX_VERASE 2
#define LINUX_VKILL 3
#define LINUX_VEOF 4
#define LINUX_VTIME 5
#define LINUX_VMIN 6
#define LINUX_VSWTC 7
#define LINUX_VSTART 8
#define LINUX_VSTOP 9
#define LINUX_VSUSP 10
#define LINUX_VEOL 11
#define LINUX_VREPRINT 12
#define LINUX_VDISCARD 13
#define LINUX_VWERASE 14
#define LINUX_VLNEXT 15
#define LINUX_VEOL2 16

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