/**
 * @file strings.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The string handling function
 * @version 0.1
 * @date 2023-10-21
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <stddef.h>

/**
 * @brief The same strlen function from OEM
 * 
 * @param s (char[]) The string to know the length of.
 * @return int 
 */
int strlen_(char s[]) {
    int i = 0;
    while (s[i] != '\0') ++i;
    return i;
}

/**
 * @brief Copies a C string (src) to another C string (dest).
 *
 * This function copies the characters from the source string `src` to the
 * destination string `dest` until a null terminator is encountered in `src`.
 *
 * @param dest The destination string where the copy will be stored.
 * @param src The source string to be copied.
 * @return A pointer to the destination string `dest`.
 */
char *strcpy(char *dest, const char *src) {
    size_t i;

    for (i = 0; src[i]; i++)
        dest[i] = src[i];

    dest[i] = 0;

    return dest;
}

/**
 * @brief Copies at most `n` characters from a source string (src) to a destination string (dest).
 *
 * This function copies at most `n` characters from the source string `src` to the
 * destination string `dest`. If `src` is shorter than `n`, it copies all characters
 * and pads the rest with null terminators.
 *
 * @param dest The destination string where the copy will be stored.
 * @param src The source string to be copied.
 * @param n The maximum number of characters to copy.
 * @return A pointer to the destination string `dest`.
 */
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    for (i = 0; i < n && src[i]; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = 0;

    return dest;
}

/**
 * @brief Compares two strings lexicographically.
 *
 * This function compares two strings `s1` and `s2` lexicographically. It returns
 * a negative value if `s1` is less than `s2`, a positive value if `s1` is greater
 * than `s2`, and zero if they are equal.
 *
 * @param s1 The first string to be compared.
 * @param s2 The second string to be compared.
 * @return An integer less than, equal to, or greater than zero, depending on the
 *         comparison result.
 */
int strcmp(const char *s1, const char *s2) {
    for (size_t i = 0; ; i++) {
        char c1 = s1[i], c2 = s2[i];
        if (c1 != c2)
            return c1 - c2;
        if (!c1)
            return 0;
    }
}

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
int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = s1[i], c2 = s2[i];
        if (c1 != c2)
            return c1 - c2;
        if (!c1)
            return 0;
    }

    return 0;
}

/**
 * @brief The OEM itoa function from C
 * 
 * @param num 
 * @param str
 * @param len
 * @param base Base of the numbers (eg. 16, 10, 8, 2)
 * @return int 
 */
int itoa(int num, unsigned char* str, int len, int base)
{
	int sum = num;
	int i = 0;
	int digit;
	if (len == 0)
		return -1;
	do
	{
		digit = sum % base;
		if (digit < 0xA)
			str[i++] = '0' + digit;
		else
			str[i++] = 'A' + digit - 0xA;
		sum /= base;
	}while (sum && (i < (len - 1)));
	if (i == (len - 1) && sum)
		return -1;
	str[i] = '\0';
	strrev(str);
	return 0;
}

/**
 * @brief The OEM strrev in C
 * 
 * @param str 
 */
void strrev(unsigned char *str)
{
	int i;
	int j;
	unsigned char a;
	unsigned len = strlen_((const char *)str);
	for (i = 0, j = len - 1; i < j; i++, j--)
	{
		a = str[i];
		str[i] = str[j];
		str[j] = a;
	}
}