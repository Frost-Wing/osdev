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

/**
 * @brief Copy memory from source to destination.
 *
 * This function copies `n` bytes of memory from the source `src` to the
 * destination `dest`. If the source and destination overlap, the behavior
 * is undefined.
 *
 * @param dest Pointer to the destination memory.
 * @param src  Pointer to the source memory.
 * @param n    Number of bytes to copy.
 * @return Pointer to the destination memory.
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * @brief Set memory to a specific value.
 *
 * This function sets the first `n` bytes of memory pointed to by `s` to the
 * specified value `c`.
 *
 * @param s Pointer to the memory to be set.
 * @param c Value to set (converted to an unsigned char).
 * @param n Number of bytes to set.
 * @return Pointer to the memory area `s`.
 */
void *memset(void *s, int c, size_t n);

/**
 * @brief Move memory from source to destination.
 *
 * This function copies `n` bytes of memory from the source `src` to the
 * destination `dest`. Unlike `memcpy`, this function handles overlapping
 * source and destination memory correctly.
 *
 * @param dest Pointer to the destination memory.
 * @param src  Pointer to the source memory.
 * @param n    Number of bytes to move.
 * @return Pointer to the destination memory.
 */
void *memmove(void *dest, const void *src, size_t n);

/**
 * @brief Compare memory areas.
 *
 * This function compares the first `n` bytes of memory in both memory areas
 * pointed to by `s1` and `s2`.
 *
 * @param s1 Pointer to the first memory area.
 * @param s2 Pointer to the second memory area.
 * @param n  Number of bytes to compare.
 * @return
 *   - 0 if the memory areas are equal.
 *   - A positive value if `s1` is greater than `s2`.
 *   - A negative value if `s1` is less than `s2`.
 */
int memcmp(const void *s1, const void *s2, size_t n);
