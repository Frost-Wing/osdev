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
#include <strings2.h>

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

char *strcpy(char *dest, const char *src) {
    size_t i;

    for (i = 0; src[i]; i++)
        dest[i] = src[i];

    dest[i] = 0;

    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    for (i = 0; i < n && src[i]; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = 0;

    return dest;
}

int strcmp(const char *s1, const char *s2) {
    for (size_t i = 0; ; i++) {
        char c1 = s1[i], c2 = s2[i];
        if (c1 != c2)
            return c1 - c2;
        if (!c1)
            return 0;
    }
}

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
int itoa(int num, string str, int len, int base)
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

/**
 * @brief Check if a substring is found within a string.
 *
 * This function searches for the presence of a substring within a given string.
 *
 * @param str The string to search within.
 * @param substr The substring to search for.
 * @return true if the substring is found in the string, false otherwise.
 */
bool contains(const char *str, const char *substr) {
    if (str == NULL || substr == NULL) {
        return false;  // Handle invalid input.
    }

    size_t str_len = strlen_(str);
    size_t substr_len = strlen_(substr);

    if (substr_len > str_len) {
        return false;  // Substring is longer than the string, so it can't be found.
    }

    for (size_t i = 0; i <= str_len - substr_len; i++) {
        if (strncmp(str + i, substr, substr_len) == 0) {
            return true;  // Substring found.
        }
    }

    return false;  // Substring not found in the string.
}

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
void string_transport_front(char *str, int x) {
    int len = strlen_(str);

    if (x >= len) {
        // Nothing to do, x is greater than or equal to the string length.
        return;
    }

    // Calculate the number of characters to delete.
    int charsToDelete = len - x;

    // Move the last x characters to the front.
    memmove(str, str + len - x, x);

    // Null-terminate the string at the new end.
    str[x] = '\0';

    // Delete the remaining characters by shifting them left.
    memmove(str + x, str + len - charsToDelete, charsToDelete);

    // Null-terminate the final string.
    str[len - charsToDelete] = '\0';
}

string trim(cstring str) {
    if (str == NULL) {
        return NULL; // Handle NULL pointer
    }

    size_t len = strlen_(str);
    string trimmedStr = (string)malloc(len + 1); // +1 for null-terminator

    if (trimmedStr == NULL) {
        return NULL; // Memory allocation failed
    }

    string dest = trimmedStr;

    for (size_t i = 0; i < len; ++i) {
        if (str[i] != ' ') {
            *dest = str[i];
            dest++;
        }
    }

    *dest = '\0';

    return trimmedStr;
}

string strcat(string dest, cstring src) {
    while (*dest != '\0') {
        dest++;
    }

    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }

    *dest = '\0';

    return dest;
}

void remove_last_char(string str) {
    size_t len = strlen_(str);
    if (len > 0) {
        str[len - 1] = '\0';
    }
}