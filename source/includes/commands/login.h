/**
 * @file login.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-16
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <basics.h>

#define MAX_USERS_ALLOWED 7
#define MAX_USERNAME_LENGTH 20
#define MAX_PASSWORD_LENGTH 40

/**
 * @brief Function to create an user.
 * 
 * @param name 
 * @param password 
 */
void create_user(char* name, char* password);

/**
 * @brief Sends a login request
 * 
 * @return char* Returns the username if the login was successful.
 */
char* login_request();