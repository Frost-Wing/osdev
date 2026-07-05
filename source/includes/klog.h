/**
 * @file klog.h
 * @brief Kernel logging subsystem backed by a ring buffer, with PIT-derived timestamps.
 * @version 0.2
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 */
#ifndef _KLOG_H
#define _KLOG_H

#include <ringbuffer.h>
#include <stdarg.h>
#include <stdbool.h>

/** Ticks per second produced by the PIT. Must match pit_freq in pit.c. */
#define KLOG_TICKS_PER_SEC 100

/** Fractional digits shown after the decimal point, e.g. [3.01] with 2 digits.
 *  Kept at 2 to match the PIT's real 10ms resolution at 100Hz - raise it only
 *  if you also raise KLOG_TICKS_PER_SEC / pit_freq. */
#define KLOG_TS_FRAC_DIGITS 2

/** Size (in bytes) of the backing klog ring buffer storage. */
#define KLOG_BUFFER_SIZE (8192 * 4)

extern bool is_klog_ready;

/**
 * @brief Initialize the klog subsystem.
 *
 * Sets up the internal ring buffer storage. Call once at boot, after init_pit().
 */
void klog_init(void);

/**
 * @brief Push a single raw character into the klog ring buffer.
 *
 * No timestamp is added. Overwrites the oldest buffered entry if full.
 *
 * @param c Character to push.
 */
void klog_putc(char c);

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
void klog_printf(cstring format, ...);

size_t klog_read(char *out, size_t max_len);

size_t klog_read_at(char *out, size_t offset, size_t max_len);

size_t klog_size(void);
;

/**
 * @brief Clear the klog ring buffer, discarding all buffered log entries.
 */
void klog_clear(void);

#endif /* _KLOG_H */