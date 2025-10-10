/**
 * @file cd.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux cd command.
 * @version 0.1
 * @date 2025-10-08
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>

int cmd_cd(int argc, char** argv)
{
    if (argc < 2) {
        printf("cd: missing operand");
        return 1;
    }

    const char* path = argv[1];

    return cd(global_fs, path);;
}
