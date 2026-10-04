/**
 * @file login.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Handle the logging-in functions.
 * @version 0.1
 * @date 2025-01-16
 *
 * @copyright Copyright (c) Pradosh 2025-2026
 *
 */

#ifndef LOGIN_H
#define LOGIN_H

#include <algorithms/hashing.h>
#include <basics.h>
#include <graphics.h>

#define MAX_USERS_ALLOWED 7
#define MAX_USERNAME_LENGTH 20
#define MAX_PASSWORD_LENGTH 40

/**
 * @brief Function to create an user with an hash.
 *
 * @param name
 * @param password
 */
void create_user(uint64 name, uint64 password);

/**
 * @brief Function to create an user with plain string.
 * @warning Potential security risk.
 *
 * @param name
 * @param password
 */
void create_user_str(cstring name, cstring password);

/**
 * @brief Read a login name into a caller-provided buffer.
 * @param userbuf Destination buffer.
 * @param max Maximum number of bytes to store.
 * @return Nonzero when a login name was entered.
 */
int login_request(char *userbuf, int max);

/**
 * @brief Requests password for verification for sudo.
 *
 * @param username current username
 * @return Whether it was successful.
 */
int ask_password(const char *username);

#endif