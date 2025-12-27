/**
 * @file lspci.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux lspci command.
 * @version 0.1
 * @date 2025-10-08
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>
#include <graphics.h>

int cmd_lspci(int argc, char** argv)
{
    print_lspci();
    return 0;
}
