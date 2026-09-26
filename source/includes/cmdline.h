/**
 * @file cmdline.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Header for cmdline.c, which handles the Limine boot cmdline and parses it for the root disk and other parameters
 * @version 0.1
 * @date 2026-09-26
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#include <basics.h>

/**
 * @brief Parses the cmdline for a specific key and returns its value
 * 
 * @param cmdline 
 * @param key 
 * @param out 
 * @param out_size 
 * @return const char* 
 */
const char *cmdline_get(const char *cmdline, const char *key, char *out, size_t out_size);