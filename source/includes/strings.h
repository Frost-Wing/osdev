/**
 * @file strings.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The header file for strings.c
 * @version 0.1
 * @date 2023-10-21
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <stddef.h>

typedef char symbol[];

/**
 * @brief Calculate the length of a null-terminated string.
 *
 * This function calculates the length of the input null-terminated string
 * by iterating through the characters until it reaches the null terminator.
 *
 * @param s The input string.
 * @return The length of the string.
 */
int strlen_(char s[]);

/**
 * @brief Copies a string from `src` to `dest`
 * 
 * @param dest Pointer to the destination
 * @param src Source string
 * @returns The resulting copy of the string `src`
 */
char *strcpy(char *dest, const char *src);

/**
 * @brief Copies `n` characters from `src` to `dest`
 * 
 * @param dest Pointer to the destination
 * @param src Source string
 * @param n Number of characters that will be copies
 * @returns The resulting copy of the string `src`
 */
char *strncpy(char *dest, const char *src, size_t n);

/**
 * @brief Compares two strings lexicographically.
 *
 * This function compares two strings `s1` and `s2` lexicographically. It returns
 * a negative value if `s1` is less than `s2`, a positive value if `s1` is greater
 * than `s2`, and zero if they are equal.
 *
 * @param s1 The first string to be compared.
 * @param s2 The second string to be compared.
 * @returns An integer less than, equal to, or greater than zero, depending on the
 *         comparison result.
 */
int strcmp(const char *s1, const char *s2);

/**
 * @brief Compares two strings, up to a specified number of characters.
 *
 * This function compares the first `n` characters of two strings `s1` and `s2`.
 * It returns a negative value if `s1` is less than `s2`, a positive value if `s1`
 * is greater than `s2`, and zero if they are equal.
 *
 * @param s1 The first string to be compared.
 * @param s2 The second string to be compared.
 * @param n The maximum number of characters to compare.
 * @return An integer less than, equal to, or greater than zero, depending on the
 *         comparison result.
 */
int strncmp(const char *s1, const char *s2, size_t n);