/**
 * @file memory.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief All memory based functions
 * @version 0.1
 * @date 2023-10-21
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <stddef.h>
#include <memory.h>

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.
 
/**
 * @brief Copies a block of memory from a source location to a destination location.
 *
 * This function copies 'n' bytes of memory from 'src' to 'dest'. The source and destination
 * memory areas should not overlap.
 *
 * @param dest Pointer to the destination memory location.
 * @param src Pointer to the source memory location.
 * @param n Number of bytes to copy.
 * @return A pointer to the destination memory location 'dest'.
 */
void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
 
    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }
 
    return dest;
}

/**
 * @brief Sets a block of memory to a specified value.
 *
 * This function sets the first 'n' bytes of the memory block at 's' to the value 'c'.
 *
 * @param s Pointer to the memory location.
 * @param c Value to set (as an integer).
 * @param n Number of bytes to set.
 * @return A pointer to the memory location 's'.
 */
void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
 
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
 
    return s;
}

/**
 * @brief Copies a block of memory from a source location to a destination location, possibly overlapping.
 *
 * This function copies 'n' bytes of memory from 'src' to 'dest'. The source and destination
 * memory areas may overlap. In such cases, it ensures the correct copy by choosing the appropriate
 * copy direction.
 *
 * @param dest Pointer to the destination memory location.
 * @param src Pointer to the source memory location.
 * @param n Number of bytes to copy.
 * @return A pointer to the destination memory location 'dest'.
 */
void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
 
    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }
 
    return dest;
}

/**
 * @brief Compares two blocks of memory.
 *
 * This function compares the first 'n' bytes of memory in 's1' and 's2'.
 *
 * @param s1 Pointer to the first memory location.
 * @param s2 Pointer to the second memory location.
 * @param n Number of bytes to compare.
 * @return 0 if the memory blocks are equal, a negative value if 's1' is less than 's2,
 * and a positive value if 's1' is greater than 's2'.
 */
int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
 
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }
 
    return 0;
}
