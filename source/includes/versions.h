/**
 * @file versions.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief This is the header file which contains all the tools used's versions.
 * @version 0.1
 * @date 2023-12-10
 *
 * @copyright Copyright (c) Pradosh 2023
 *
 */
#include <basics.h>
#include <graphics.h>

/**
 * @brief Contains GCC, CC, LD, MAKE, xorriso, tar versions.
 *
 */
extern cstring versions __attribute__((weak));

/**
 * @brief Contains the exact time when the compilation started.
 *
 */
extern cstring date     __attribute__((weak));

static inline void frost_compilation_information(void)
{
    LOG_SCOPE();

    /* Symbols were not provided by the linker */
    if (&versions == NULL || &date == NULL)
        return;

    /* Symbols exist, but contain NULL */
    if (versions == NULL || date == NULL)
        return;

    char line_buf[256];
    const char *line = versions;

    while (*line) {
        size_t len = 0;

        while (line[len] && line[len] != '\n')
            len++;

        if (len >= sizeof(line_buf))
            len = sizeof(line_buf) - 1;

        strncpy(line_buf, line, len);
        line_buf[len] = '\0';

        info("%s", __FILE__, line_buf);

        line += len;

        if (*line == '\n')
            line++;
    }

    info("Compiled Time (Started at): %s", __FILE__, date);
}