/**
 * @file ls.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux ls command.
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>

int cmd_ls(int argc, char** argv)
{
    list_contents(global_fs);
    return 0;
}
