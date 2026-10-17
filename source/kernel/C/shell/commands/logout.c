#include <commands/commands.h>
#include <sh_util.h>

extern bool running;

int cmd_logout(int argc, char **argv) {
    (void)argc;
    (void)argv;
    running = false;
    return 0;
}
