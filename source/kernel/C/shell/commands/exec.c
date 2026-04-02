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
#include <multitasking.h>

int cmd_exec(int argc, char** argv)
{
    if (argc < 2) {
        eprintf("exec: missing program");
        eprintf("usage: exec <program> [args]");
        return 1;
    }

    user_task_spec_t spec;
    memset(&spec, 0, sizeof(spec));

    const char* path = argv[1];
    spec.path = path;

    int user_argc = 0;
    for (int i = 2; i < argc && user_argc < 31; ++i)
        spec.argv[user_argc++] = argv[i];

    spec.argc = user_argc;
    spec.argv[user_argc] = NULL;

    tty_flush_input();
    keyboard_flush_buffer();

    uint32_t pid = multitasking_spawn_userland(path, &spec);
    if (pid == 0) {
        eprintf("exec: failed to create task");
        return -1;
    }

    printf("exec: scheduled '%s' as pid %u", path, pid);
    return 0;
}
