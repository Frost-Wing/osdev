/**
 * @file probepci.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Custom probepci command
 * @version 0.1
 * @date 2025-10-08
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>
#include <graphics.h>
#include <pci.h>

int cmd_probepci(int argc, char** argv)
{
    probe_pci();
    return 0;
}
