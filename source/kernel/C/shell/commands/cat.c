/**
 * @file cat.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux cat command.
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>

int cmd_cat(int argc, char** argv)
{
    if (argc < 2) {
        printf("cat: missing file operand");
        return 1;
    }
    
    printf("%s", read_file(global_fs, argv[1]));

    return 0;
}
