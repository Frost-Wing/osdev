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
#include <graphics.h>
#include <tty.h>
#include <keyboard.h>

extern char* global_envp[];

int cmd_exec(int argc, char** argv)
{
    if (argc < 2) {
        eprintf("exec: missing program");
        eprintf("usage: exec <program> [args]");
        return 1;
    }

    const char* path = argv[1];

    const char* user_argv[34];
    int user_argc = 0;

    // argv[0] should be the program name (basename)
    const char* basename = vfs_basename(path);
    // user_argv[user_argc++] = basename;

    // copy actual arguments AFTER the program path
    for (int i = 2; i < argc && user_argc < 33; ++i)
        user_argv[user_argc++] = argv[i];

    user_argv[user_argc] = NULL;

    tty_flush_input();
    keyboard_flush_buffer();

    if (userland_exec(path, user_argc, user_argv, global_envp) != 0) {
        keyboard_flush_buffer();
        eprintf("exec: failed to load ELF");
        return -1;
    }

    keyboard_flush_buffer();
    return 0;
}
