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
        printf("rm: missing operand");
        return 1;
    }

    int recursive = 0;
    int start = 1;

    if (strcmp(argv[1], "-r") == 0) {
        recursive = 1;
        start = 2;

        if (argc < 3) {
            printf("rm: missing operand after '-r'");
            return 1;
        }
    }

    for (int i = start; i < argc; i++) {
        int ret;

        if (recursive)
            ret = vfs_rm_recursive(argv[i]);   // recursive delete
        else
            ret = vfs_unlink(argv[i]);  // file only

        if (ret != 0)
            printf("rm: cannot remove '%s'", argv[i]);
    }

    return 0;
}