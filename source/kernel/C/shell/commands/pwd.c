/**
 * @file pwd.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The basic linux pwd
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>

int cmd_pwd(int argc, char** argv)
{
    pwd(global_fs);
    return 0;
}
 