/**
 * @file sort.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Declarations for sorting algorithms.
 * @version 0.1
 * @date 2026-04-07
 *
 * @copyright Copyright (c) Pradosh 2026
 *
 */
#ifndef SORT_H
#define SORT_H

#include <basics.h> /* size_t */

/**
 * @brief Comparison callback for qsort().
 *
 * Return <0 if a sorts before b, 0 if equal, >0 if a sorts after b.
 */
typedef int (*qsort_cmp_fn)(const void *a, const void *b);

/**
 * @brief Sort an array in place (quicksort, not stable).
 *
 * @param base  Pointer to the first element.
 * @param nmemb Number of elements.
 * @param size  Size of each element in bytes.
 * @param cmp   Comparison callback.
 */
void qsort(void *base, size_t nmemb, size_t size, qsort_cmp_fn cmp);

#endif /* SORT_H */