/**
 * @file shutdown.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The basic Linux shutdown command.
 * @version 0.1
 * @date 2025-10-08
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>

int cmd_shutdown(int argc, char** argv)
{
    if (argc < 2) {
        printf("shutdown: missing operand (try 'shutdown -h now' or 'shutdown -r now')");
        return 1;
    }

    int do_halt = 0;
    int do_reboot = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            do_halt = 1;
        } else if (strcmp(argv[i], "-r") == 0) {
            do_reboot = 1;
        } else if (strcmp(argv[i], "now") == 0) {
            continue; // ignore "now" keyword for syntax parity
        } else {
            printf("shutdown: unrecognized option '%s'", argv[i]);
            return 1;
        }
    }

    if (do_reboot)
        reboot();
    else if (do_halt)
        shutdown();
    else
        printf("shutdown: missing action (use -h or -r)");

    return 0;
}
