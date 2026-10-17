#include <commands/commands.h>
#include <commands/login.h>

int cmd_useradd(int argc, char **argv) {
    if (login_current_uid() != 0) {
        eprintf("useradd: permission denied");
        return 1;
    }
    if (argc != 2) {
        eprintf("usage: useradd <username>");
        return 2;
    }
    if (login_create_user(argv[1]) != 0) {
        eprintf("useradd: could not create account");
        return 1;
    }
    printf("Created account %s", argv[1]);
    return 0;
}
