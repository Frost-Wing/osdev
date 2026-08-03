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
#include <graphics.h>
#include <keyboard.h>
#include <multitasking.h>
#include <strings.h>
#include <tty.h>
#include <userland.h>

extern char *global_envp[];

int cmd_exec(int argc, char **argv) {
    if (argc < 2) {
        eprintf("exec: missing program");
        eprintf("usage: exec <program> [args]");
        return 1;
    }

    const char *path = argv[1];

    const char *user_argv[34];
    int user_argc = 0;

    /*
     * Toybox selects an applet from argv[0] when invoked as
     * `/bin/toybox <applet>`, so keep the historical shell behaviour of
     * passing only the words after the executable path. For example:
     *   exec /bin/toybox whoami  -> argv[0] = "whoami"
     *   exec /bin/toybox sh      -> argv[0] = "sh"
     */
    for (int i = 2; i < argc && user_argc < 31; ++i)
        user_argv[user_argc++] = argv[i];

    user_argv[user_argc] = NULL;

    tty_flush_input();
    keyboard_flush_buffer();

    user_task_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.path = path;
    spec.argc = user_argc;
    spec.parent_pid = 0;

    for (int i = 0; i < user_argc; i++)
        spec.argv[i] = user_argv[i];

    uint32_t pid = multitasking_spawn_userland(path, &spec);
    if (!pid) {
        keyboard_flush_buffer();
        eprintf("exec: failed to spawn process");
        return -1;
    }

    int exit_code = 0;
    while (true) {
        task_info_t info;
        if (!multitasking_get_task(pid, &info))
            break;

        if (info.state == TASK_STATE_EXITED) {
            task_info_t reaped;
            if (multitasking_reap_task(pid, &reaped))
                exit_code = reaped.exit_code;
            else
                exit_code = info.exit_code;
            break;
        }

        multitasking_pump();
    }

    keyboard_flush_buffer();
    return exit_code;
}
