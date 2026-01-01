/**
 * @file mv.c
 * @author Pradosh
 * @brief Basic linux mv command.
 * @version 0.1
 * @date 2025-10-07
 */

#include <commands/commands.h>

int cmd_mv(int argc, char** argv)
{
    if (argc < 3) {
        printf("mv: missing file operand");
        return 1;
    }

    if (argc > 3) {
        printf("mv: too many arguments");
        return 1;
    }

    const char* src = argv[1];
    const char* dst = argv[2];

    int ret = vfs_mv(src, dst);
    if (ret != 0) {
        printf("mv: failed to move '%s' to '%s'", src, dst);
        return 1;
    }

    return 0;
}
