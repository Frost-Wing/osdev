/**
 * @file exec.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Executes an ELF from VFS under userland
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>
#include <executables/elf.h>
#include <userland.h>

int cmd_exec(int argc, char** argv)
{
    if (argc < 2) {
        printf("exec: missing program");
        printf("usage: exec <program> [args]");
        return 1;
    }

    const char* path = argv[1];
    const char* user_argv[34];
    int user_argc = 0;

    for (int i = 1; i < argc && user_argc < 33; ++i)
        user_argv[user_argc++] = argv[i];

    const char* basename = vfs_basename(path);
    if (user_argc == 1 &&
        (strcmp(basename, "busybox") == 0 || strcmp(basename, "BUSYBOX") == 0))
        user_argv[user_argc++] = "ash";

    user_argv[user_argc] = NULL;

    if (userland_exec(path, user_argc, user_argv, NULL) != 0) {
        printf("exec: failed to load ELF");
        return -1;
    }

    return 0;
}
