/**
 * @file rm.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux rm code.
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>

int cmd_rm(int argc, char** argv)
{
    if (argc < 2) {
        printf("rm: missing file operand");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        delete_file(global_fs, argv[i]);
    }

    return 0;
}
