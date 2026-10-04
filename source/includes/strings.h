/**
 * @file strings.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The header file for strings.c
 * @version 0.1
 * @date 2023-10-21
 *
 * @copyright Copyright (c) Pradosh 2023-2026
 *
 */
#ifndef STRINGS_H
#define STRINGS_H

#include <basics.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_WORDS 30
#define MAX_WORD_LEN 40

#define CONCAT(...) \
    str_concat_impl(sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *), __VA_ARGS__)

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
int strlen(cstring s);

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
 * @brief Find first occurrence of substring `needle` in `haystack`
 * @return pointer to the start of the match in haystack, or NULL if not found
 */
char *strstr(const char *haystack, const char *needle);

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

/**
 * @brief Converts a string to a long integer.
 *
 * @param str The input string to be converted.
 * @param endptr Reference to a pointer that will be updated to point to the character after the last valid character.
 * @param base The base of the number.
 * @return long
 */
long strtol(const char *str, char **endptr, int base);

/**
 * @brief Converts an uint to string
 *
 * @param num
 * @return char*
 */
char *uint_to_string(unsigned int num);

/**
 * @brief Prints Hexadecimal number
 *
 * @param hex the hexadecimal number to be printed.
 */
char *hex_to_string(signed int num, bool caps);

/**
 * @brief Removes the leading and trailing spaces.
 *
 * @param str
 * @return char*
 */
char *leading_trailing_trim(const char *str);

/**
 * @brief Splits a string into tokens based on a delimiter.
 *
 * @param str The input string to be split.
 * @param delim The delimiter character.
 * @param num_tokens The number of tokens found.
 * @return char** Returns an array of strings containing the tokens.
 */
char **splitf(const char *str, char delim, int *num_tokens);

/**
 * @brief Implemented for sh.c
 *
 * @param c
 * @return int
 */
int isspace(char c);

/**
 * @brief Remove leading and trailing whitespace from a string in place.
 *
 * @param s String to trim.
 * @return The trimmed string.
 */
char *trim_inplace(char *s);

/**
 * @brief Find the last occurrence of a character in a string.
 *
 * @param s String to search.
 * @param c Character to find.
 * @return A pointer to the matching character, or NULL if not found.
 */
char *strrchr(const char *s, int c);

/**
 * @brief Allocate and return a duplicate of a string.
 *
 * @param str String to duplicate.
 * @return The allocated duplicate, or NULL if allocation fails.
 */
char *strdup(const char *str);

/**
 * @brief Find the first occurrence of a character in a string.
 *
 * @param s String to search.
 * @param c Character to find.
 * @return A pointer to the matching character, or NULL if not found.
 */
char *strchr(const char *s, int c);

/**
 * @brief Convert an integer to a string in the requested base.
 *
 * @param num Value to convert.
 * @param str Destination buffer.
 * @param len Size of the destination buffer.
 * @param base Numeric base for conversion.
 * @return 0 on success, nonzero if the value cannot be represented.
 */
int itoa(int num, string str, int len, int base);

/**
 * @brief Convert an ASCII lowercase letter to uppercase.
 *
 * @param c Character to convert.
 * @return The converted character, or @p c if it is not lowercase.
 */
char toupper(char c);

/**
 * @brief Reverse a null-terminated string in place.
 *
 * @param str String to reverse.
 */
void strrev(unsigned char *str);

/**
 * @brief Check whether a string begins with a prefix.
 *
 * @param str String to inspect.
 * @param prefix Prefix to match.
 * @return true if @p str begins with @p prefix, otherwise false.
 */
bool starts_with(const char *str, const char *prefix);

/**
 * @brief Convert a 64-bit integer to a hexadecimal string.
 *
 * @param num Value to convert.
 * @return A pointer to a static buffer containing the result.
 */
char *uint64_to_hex(uint64_t num);

/**
 * @brief Split a string on spaces into bounded words.
 *
 * @param str Input string.
 * @param words Output array with MAX_WORDS rows of MAX_WORD_LEN characters.
 * @param num_words Receives the number of words written.
 */
void split(const char *str, char words[][MAX_WORD_LEN], int *num_words);

/**
 * @brief Split a string on a delimiter into bounded words.
 *
 * @param str Input string.
 * @param words Output array with MAX_WORDS rows of MAX_WORD_LEN characters.
 * @param num_words Receives the number of words written.
 * @param delimiter Character separating words.
 */
void splitw(const char *str, char words[][MAX_WORD_LEN], int *num_words, char delimiter);

/**
 * @brief Compare two strings without regard to ASCII letter case.
 *
 * @param a First string.
 * @param b Second string.
 * @return A value less than, equal to, or greater than zero as the strings compare.
 */
int strcasecmp(const char *a, const char *b);

/**
 * @brief Concatenate a counted list of strings into a newly allocated string.
 *
 * @param count Number of string arguments.
 * @return The allocated concatenated string, or NULL if allocation fails.
 */
char *str_concat_impl(int count, ...);

/**
 * @brief Extract the next token from a string using the given delimiters.
 *
 * @param str String to tokenize, or NULL to continue the previous scan.
 * @param delim Set of delimiter characters.
 * @param saveptr State pointer used between calls.
 * @return The next token, or NULL if no tokens remain.
 */
char *strtok_r(char *str, const char *delim, char **saveptr);

/**
 * @brief Extract the next token using static tokenizer state.
 *
 * @param str String to tokenize, or NULL to continue the previous scan.
 * @param delim Set of delimiter characters.
 * @return The next token, or NULL if no tokens remain.
 */
char *strtok(char *str, const char *delim);

#endif // STRINGS_H
