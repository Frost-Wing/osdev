/**
 * @file sort.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Contains code for sorting algorithms.
 * @version 0.1
 * @date 2026-04-07
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#include <basics.h>

typedef int (*qsort_cmp_fn)(const void*, const void*);

static void swap_bytes(char* a, char* b, size_t size)
{
    while (size--) {
        char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

static int partition(char* base, int low, int high, size_t size, qsort_cmp_fn cmp)
{
    char* pivot = base + high * size;
    int i = low - 1;

    for (int j = low; j < high; j++) {
        char* elem = base + j * size;

        if (cmp(elem, pivot) < 0) {
            i++;
            swap_bytes(base + i * size, elem, size);
        }
    }

    swap_bytes(base + (i + 1) * size, base + high * size, size);
    return i + 1;
}

static void quicksort(char* base, int low, int high, size_t size, qsort_cmp_fn cmp)
{
    if (low < high) {
        int pi = partition(base, low, high, size, cmp);

        quicksort(base, low, pi - 1, size, cmp);
        quicksort(base, pi + 1, high, size, cmp);
    }
}

void qsort(void* base, size_t nmemb, size_t size, qsort_cmp_fn cmp)
{
    if (!base || nmemb < 2)
        return;

    quicksort((char*)base, 0, nmemb - 1, size, cmp);
}