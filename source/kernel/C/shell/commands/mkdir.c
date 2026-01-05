/**
 * @file mkdir.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux mkdir command.
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */
#include <commands/commands.h>

int cmd_mkdir(int argc, char** argv)
{
    if (argc < 2) {
        printf("mkdir: missing operand");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        VFS_CREATEe_path(argv[i], 0x10);
    }

    return 0;
}
