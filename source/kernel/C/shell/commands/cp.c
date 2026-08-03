/**
 * @file cp.c
 * @author Pradosh
 * @brief Basic linux cp command.
 * @version 0.1
 * @date 2025-10-07
 */

#include <commands/commands.h>

int cmd_cp(int argc, char **argv) {
    if (argc < 3) {
        printf("cp: missing file operand");
        return 1;
    }

    if (argc > 3) {
        printf("cp: too many arguments");
        return 1;
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    int ret = vfs_cp(src, dst);
    if (ret != 0) {
        printf("cp: failed to copy '%s' to '%s'", src, dst);
        return 1;
    }

    return 0;
}