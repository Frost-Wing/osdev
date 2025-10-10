/**
 * @file whoami.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux whoami command.
 * @version 0.1
 * @date 2025-10-08
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>
#include <graphics.h>

extern string current_user;

int cmd_whoami(int argc, char** argv)
{
    if (!current_user) {
        printf("unknown");
        return 0;
    }

    if (strcmp(current_user, "root") == 0) {
        print(red_color);
        print(current_user);
        print(reset_color);
    } else {
        print(green_color);
        print(current_user);
        print(reset_color);
    }

    print("\n");
    return 0;
}
