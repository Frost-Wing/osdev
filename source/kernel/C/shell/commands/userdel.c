#include <commands/commands.h>
#include <commands/login.h>

int cmd_userdel(int argc, char **argv) {
    if (login_current_uid() != 0) {
        eprintf("userdel: permission denied");
        return 1;
    }
    if (argc != 2) {
        eprintf("usage: userdel <username>");
        return 2;
    }
    if (login_remove_user(argv[1]) != 0) {
        eprintf("userdel: could not remove account");
        return 1;
    }
    printf("Removed account %s", argv[1]);
    return 0;
}
