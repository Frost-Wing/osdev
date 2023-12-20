/**
 * @file hashing.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Contains header for hashing and encrypting
 * @version 0.1
 * @date 2023-12-20
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>

/**
 * @brief Verifies if the hash is working (not really initializing anything)
 * 
 */
void init_hashing();

/**
 * @brief The function to hash an string (const char *)
 * 
 * @param data The string to be hashed.
 * @return [int64] Hashed value.
 */
int64 hash_string(string data);