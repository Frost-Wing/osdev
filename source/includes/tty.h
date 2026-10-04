/**
 * @file tty.h
 * @brief Kernel terminal input, output, and configuration interfaces.
 */
#ifndef TTY_H
#define TTY_H

#include <basics.h>
#include <sys/termios.h>
#include <stdint.h>
#include <stdbool.h>

struct flanterm_context;

#define TTY_LINE_MAX 256
#define TTY_COOKED_MAX 1024
#define TTY_COUNT 1
/* Ask the task layer to inherit its parent's terminal. */
#define TTY_INDEX_CURRENT UINT8_MAX

/** @brief Initialize the kernel terminal subsystem. */
void tty_init(void);
/**
 * @brief Attach tty0 to the initialized framebuffer console.
 *
 * The extra parameters preserve compatibility with existing callers; no
 * additional terminals are created.
 *
 * @param default_terminal Initialized framebuffer terminal context.
 * @param framebuffer Framebuffer address.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @param pitch Framebuffer pitch in bytes.
 * @param ssfn_font Font data.
 * @param ssfn_font_size Font data size in bytes.
 * @return true on success, otherwise false.
 */
bool tty_init_terminals(struct flanterm_context *default_terminal,
                        uint32_t *framebuffer, size_t width, size_t height,
                        size_t pitch, const void *ssfn_font,
                        size_t ssfn_font_size);

/** @brief Queue a character as terminal input. */
void tty_input_char(char c);

/**
 * @brief Read input from the active terminal.
 * @param buf Destination buffer.
 * @param count Maximum number of bytes to read.
 * @return Number of bytes read, or a negative error code.
 */
int tty_read(char *buf, uint64_t count);

/** @brief Discard buffered terminal input. */
void tty_flush_input(void);

/** @brief Check whether a terminal interrupt is pending. */
bool tty_interrupt_pending(void);

/** @brief Consume the pending terminal interrupt, if present. */
int tty_take_interrupt(void);

/** @brief Clear the pending terminal interrupt. */
void tty_clear_interrupt(void);

/** @brief Map a terminal control key to the task exit code it requests. */
int tty_process_exit_code_for_key(int key);

/**
 * @brief Get the active terminal's termios settings.
 * @param termios Receives the settings.
 * @return true on success, otherwise false.
 */
bool tty_get_termios(linux_termios_t *termios);

/**
 * @brief Set the active terminal's termios settings.
 * @param termios Settings to apply.
 * @param flush_input Whether to discard pending input.
 * @return true on success, otherwise false.
 */
bool tty_set_termios(const linux_termios_t *termios, bool flush_input);

/**
 * @brief Get the active terminal's window size.
 * @param winsize Receives the window dimensions.
 * @return true on success, otherwise false.
 */
bool tty_get_winsize(linux_winsize_t *winsize);

/**
 * @brief Switch to a terminal by index.
 * @param index Terminal index.
 * @return true on success, otherwise false.
 */
bool tty_switch(uint8_t index);

/** @brief Return the active terminal index. */
uint8_t tty_active_index(void);

/** @brief Queue a key event as terminal input. */
void tty_input_key(int key);

#endif