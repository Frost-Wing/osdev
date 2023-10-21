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

/**
 * @brief The same strlen function from OEM
 * 
 * @param s (char[]) The string to know the length of.
 * @return int 
 */
int strlen(char s[]) {
    int i = 0;
    while (s[i] != '\0') ++i;
    return i;
}