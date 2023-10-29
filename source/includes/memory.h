/**
 * @file memory.h
 * @brief Custom memory manipulation functions.
 *
 * This header file defines custom memory manipulation functions that are
 * equivalent to the standard C library functions `memcpy`, `memset`,
 * `memmove`, and `memcmp`.
 */

#include <stdint.h>
#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memcpy64(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);