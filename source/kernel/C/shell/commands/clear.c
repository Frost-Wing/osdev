/**
 * @file clear.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux clear command.
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>

int cmd_clear(int argc, char** argv)
{
    print("\033[2J\033[H"); // clear & return to home.
    return 0;
}
