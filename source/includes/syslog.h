/**
 * @file syslog.h
 * @brief Syscall logging subsystem backed by a ring buffer, with PIT-derived timestamps.
 * @version 0.2
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 */
#ifndef SYSLOG_H
#define SYSLOG_H

#include <ringbuffer.h>
#include <stdarg.h>
#include <stdbool.h>

/** Ticks per second produced by the PIT. Must match pit_freq in pit.c. */
#define SYSLOG_TICKS_PER_SEC 100

/** Fractional digits shown after the decimal point, e.g. [3.01] with 2 digits.
 *  Kept at 2 to match the PIT's real 10ms resolution at 100Hz - raise it only
 *  if you also raise SYSLOG_TICKS_PER_SEC / pit_freq. */
#define SYSLOG_TS_FRAC_DIGITS 2

/** Size (in bytes) of the backing syslog ring buffer storage. */
#define SYSLOG_BUFFER_SIZE (8192)

extern bool is_syslog_ready;

/**
 * @brief Initialize the syslog subsystem.
 *
 * Sets up the internal ring buffer storage. Call once at boot, after init_pit().
 */
void syslog_init(void);

/**
 * @brief Push a single raw character into the syslog ring buffer.
 *
 * No timestamp is added. Overwrites the oldest buffered entry if full.
 *
 * @param c Character to push.
 */
void syslog_putc(char c);

/**
 * @brief Formatted kernel log write, automatically prefixed with a
 *        "[seconds.fraction] " timestamp (derived from pit_ticks) and
 *        suffixed with a newline.
 *
 * Supports %d, %u, %x, %X, %s, %c, %% format specifiers, with optional
 * zero-padding / width (e.g. %04d), matching the style used by printf_internal
 * in logger.c.
 *
 * @param format printf-style format string.
 * @param ...    Arguments matching the format string.
 */
void syslog_printf(cstring format, ...);

size_t syslog_read(char* out, size_t max_len);

size_t syslog_read_at(char* out, size_t offset, size_t max_len);

size_t syslog_size(void);;

/**
 * @brief Clear the syslog ring buffer, discarding all buffered log entries.
 */
void syslog_clear(void);

#endif /* SYSLOG_H */