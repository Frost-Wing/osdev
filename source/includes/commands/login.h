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
 * Load account names from /etc/passwd and password verifiers from /etc/shadow,
 * or perform first-boot root setup on a writable FAT16, FAT32, or ext2 root
 * filesystem. Existing /etc/fwauth.dat credentials are migrated on first boot.
 * Shadow verifiers use FrostWing's $fwauth$ format and never store plaintext
 * passwords. Ext2 stores /etc/shadow with mode 0600; FAT relies on kernel
 * access checks instead.
 */
int login_setup_first_user(void);

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