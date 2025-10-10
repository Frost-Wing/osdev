/**
 * @file echo.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux echo command.
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>

int cmd_echo(int argc, char** argv)
{
    bool newline = true;
    int start = 1;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = false;
        start = 2;
    }

    for (int i = start; i < argc; i++) {
        printfnoln("%s", argv[i]);
        if (i < argc - 1) putc(' ');
    }

    if (newline) putc('\n');
    return 0;
}
