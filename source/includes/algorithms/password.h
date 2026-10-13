#ifndef PASSWORD_H
#define PASSWORD_H

#include <stddef.h>
#include <stdint.h>

#define PASSWORD_SALT_SIZE 16
#define PASSWORD_HASH_SIZE 32

void password_derive_key(const uint8_t *password, size_t password_len,
    const uint8_t salt[PASSWORD_SALT_SIZE], uint32_t iterations,
    uint8_t out[PASSWORD_HASH_SIZE]);

#endif
