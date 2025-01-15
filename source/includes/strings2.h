/**
 * @file strings2.h
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
#include <stdbool.h>
#include <basics.h>

// typedef char symbol[];

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
string strcpy(string dest, cstring src);

/**
 * @brief Copies `n` characters from `src` to `dest`
 * 
 * @param dest Pointer to the destination
 * @param src Source string
 * @param n Number of characters that will be copies
 * @returns The resulting copy of the string `src`
 */
string strncpy(string dest, cstring src, size_t n);

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
int strcmp(cstring s1, cstring s2);

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
int strncmp(cstring s1, cstring s2, size_t n);

/**
 * @brief Check if a substring is found within a string.
 *
 * This function searches for the presence of a substring within a given string.
 *
 * @param str The string to search within.
 * @param substr The substring to search for.
 * @return true if the substring is found in the string, false otherwise.
 */
bool contains(cstring str, cstring substr);

/**
 * @brief Move the last x characters of a string to the front and delete the rest.
 *
 * This function takes a null-terminated C string and moves the last x characters to
 * the beginning of the string while deleting the remaining characters. If x is greater
 * than or equal to the length of the string, the function does nothing.
 *
 * @param str The input string.
 * @param x The number of characters to move to the front.
 *
 * @note This function modifies the input string in place.
 */
void string_transport_front(char *str, int x);

/**
 * @brief Creates a new string without spaces from a C-style string.
 *
 * This function allocates memory for a new string without spaces
 * and returns the result. It is the caller's responsibility to free
 * the allocated memory.
 *
 * @param str The input string to be trimmed.
 * @return A dynamically allocated string without spaces.
 */
string trim(cstring str);

/**
 * @brief Concatenate two strings.
 *
 * This function concatenates the source string @p src to the end of the
 * destination string @p dest. It assumes that @p dest has enough space to
 * accommodate the concatenated result.
 *
 * @param dest The destination string.
 * @param src The source string to be concatenated.
 * @return A pointer to the destination string.
 */
string strcat(string dest, cstring src);

/**
 * @brief Removes the last char
 * 
 * @param str 
 */
void remove_last_char(string str);

long strtol(const char *str, char **endptr, int base);

/**
 * @brief Converts an uint to string
 * 
 * @param num 
 * @return char* 
 */
 char* uint_to_string(unsigned int num);


 char** splitf(const char* str, char delim, int* num_tokens);