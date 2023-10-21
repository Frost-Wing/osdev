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
/**
 * @brief Calculate the length of a null-terminated string.
 *
 * This function calculates the length of the input null-terminated string
 * by iterating through the characters until it reaches the null terminator.
 *
 * @param s The input string.
 * @return The length of the string.
 */
int strlen(char s[]);