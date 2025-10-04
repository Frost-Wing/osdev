/**
 * @file login.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Handle the logging-in functions.
 * @version 0.1
 * @date 2025-01-16
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <basics.h>
#include <algorithms/hashing.h>

#define MAX_USERS_ALLOWED 7
#define MAX_USERNAME_LENGTH 20
#define MAX_PASSWORD_LENGTH 40

/**
 * @brief Function to create an user with an hash.
 * 
 * @param name 
 * @param password 
 */
void create_user(int64 name, int64 password);

/**
 * @brief Function to create an user with plain string.
 * @warning Potential security risk. 
 *
 * @param name 
 * @param password 
 */
void create_user_str(cstring name, cstring password);

/**
 * @brief Sends a login request
 * 
 * @return char* Returns the username if the login was successful.
 */
char* login_request();

/**
 * @brief Requests password for verification for sudo.
 * 
 * @param username current username
 * @return Whether it was successful.
 */
int ask_password(const char* username);