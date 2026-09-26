/**
 * @file cmdline.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Handles the Limine boot cmdline and parses it for the root disk and other parameters
 * @version 0.1
 * @date 2026-09-26
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#include <cmdline.h>
#include <basics.h>
#include <strings.h>
#include <memory.h>

const char *cmdline_get(const char *cmdline, const char *key, char *out, size_t out_size) {
    if (!cmdline || !key || !out || out_size == 0)
        return NULL;

    size_t key_len = strlen(key);
    const char *p = cmdline;

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        const char *tok_start = p;
        while (*p && *p != ' ') p++;
        size_t tok_len = (size_t)(p - tok_start);

        if (tok_len > key_len && tok_start[key_len] == '=' &&
            strncmp(tok_start, key, key_len) == 0) {
            size_t val_len = tok_len - key_len - 1;
            if (val_len >= out_size)
                val_len = out_size - 1;
            memcpy(out, tok_start + key_len + 1, val_len);
            out[val_len] = '\0';
            return out;
        }
    }
    return NULL;
}