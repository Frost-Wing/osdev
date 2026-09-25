#include "internal.h"

uint64 sys_execve(const char *target,
    char *const *argv,
    char *const *envp) {
    if (!target)
        return -LINUX_EINVAL;

    char **copied_argv = NULL;
    char **copied_envp = NULL;

    int argc = copy_user_string_array(argv, &copied_argv);
    if (argc < 0)
        return argc;

    int envc = copy_user_string_array(envp, &copied_envp);
    if (envc < 0) {
        free_copied_string_array(copied_argv, argc);
        return envc;
    }

    userland_exec_ctx_t ctx;

    ctx.path = target;
    ctx.argc = argc;
    ctx.envp = (const char *const *)copied_envp;

    for (int i = 0; i < argc && i < 31; i++)
        ctx.argv[i] = copied_argv[i];

    ctx.argv[argc] = NULL;

    int rc;
    uint32_t current_pid = multitasking_current_pid();

    if (should_route_to_toybox(target)) {
        const char *toybox_argv[32];
        int toybox_argc = build_toybox_argv(
            target,
            argc,
            (char **)copied_argv,
            toybox_argv);

        userland_exec_ctx_t tb_ctx;

        tb_ctx.path = "/bin/toybox";
        tb_ctx.argc = toybox_argc;
        tb_ctx.envp = ctx.envp;

        for (int i = 0; i < toybox_argc && i < 32; i++)
            tb_ctx.argv[i] = toybox_argv[i];

        tb_ctx.argv[toybox_argc] = NULL;

        if (current_pid && !multitasking_update_user_image(current_pid, tb_ctx.path, tb_ctx.argc, tb_ctx.argv)) {
            free_copied_string_array(copied_argv, argc);
            free_copied_string_array(copied_envp, envc);
            return -LINUX_ENOMEM;
        }

        rc = userland_exec_replace(&tb_ctx);
    } else {
        if (!path_is_loadable_elf(ctx.path)) {
            free_copied_string_array(copied_argv, argc);
            free_copied_string_array(copied_envp, envc);
            return -LINUX_ENOEXEC;
        }

        if (current_pid && !multitasking_update_user_image(current_pid, ctx.path, ctx.argc, ctx.argv)) {
            free_copied_string_array(copied_argv, argc);
            free_copied_string_array(copied_envp, envc);
            return -LINUX_ENOMEM;
        }

        rc = userland_exec_replace(&ctx);
    }

    free_copied_string_array(copied_argv, argc);
    free_copied_string_array(copied_envp, envc);

    return (rc == 0) ? 0 : -LINUX_ENOEXEC;
}

uint64 sys_fork(void) {
    if (multitasking_current_is_fork_child())
        return 0;

    task_t *cur = multitasking_get_current_task();
    if (!cur) {
        debug_printf("[syscall] failed, could not get the current task! (fork())\n");
        syslog_printf("[syscall] failed, could not get the current task! (fork())");
        return -LINUX_ENOSYS;
    }

    if (!cur->user_spec.path) {
        debug_printf("[syscall] user_spec.path is null!\n");
        syslog_printf("[syscall] user_spec.path is null!");
        return -LINUX_ENOSYS;
    }

    const char *spawn_path = cur->user_spec.path;

    const char *spawn_argv[32];
    int spawn_argc = cur->user_spec.argc;

    if (spawn_argc < 0)
        spawn_argc = 0;
    if (spawn_argc > 31)
        spawn_argc = 31;

    for (int i = 0; i < spawn_argc; i++)
        spawn_argv[i] = cur->user_spec.argv[i] ? cur->user_spec.argv[i] : "";

    spawn_argv[spawn_argc] = NULL;

    // toybox routing
    if (should_route_to_toybox(spawn_path)) {
        const char *toybox_argv[32];
        int toybox_argc = build_toybox_argv(
            spawn_path,
            spawn_argc,
            spawn_argv,
            toybox_argv);

        spawn_path = "/bin/toybox";
        spawn_argc = toybox_argc;

        for (int i = 0; i < toybox_argc && i < 32; i++)
            spawn_argv[i] = toybox_argv[i];
    }

    if (!path_is_loadable_elf(spawn_path))
        return -LINUX_ENOEXEC;

    user_task_spec_t spec;
    memset(&spec, 0, sizeof(spec));

    spec.path = spawn_path;
    spec.argc = spawn_argc;
    spec.parent_pid = cur->pid;
    spec.fork_child = true;

    for (int i = 0; i < spawn_argc; i++)
        spec.argv[i] = spawn_argv[i];

    uint32_t child = multitasking_spawn_userland(spawn_path, &spec);

    if (!child)
        return -LINUX_EAGAIN;

    return child;
}

uint64 sys_wait4(int64_t pid, int *status, int options, void *rusage) {
    (void)rusage;
    const int LINUX_WNOHANG = 1;

    if ((options & ~LINUX_WNOHANG) != 0)
        return -LINUX_EINVAL;

    uint32_t parent = multitasking_current_pid();
    if (parent == 0)
        parent = 1;

    while (1) {
        task_info_t info = {0};
        if (multitasking_find_child(parent, pid, true, &info)) {
            if (!multitasking_reap_task(info.pid, &info))
                continue;

            if (status)
                *status = (info.exit_code & 0xFF) << 8;
            return (uint64)info.pid;
        }

        if (!multitasking_find_child(parent, pid, false, NULL))
            return -LINUX_ECHILD;
        if (options & LINUX_WNOHANG)
            return 0;

        multitasking_pump();
    }
}

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

uint64 sys_futex(uint32_t *uaddr, int op, uint32_t val,
    const linux_timespec_t *timeout,
    uint32_t *uaddr2, uint32_t val3) {
    (void)timeout;
    (void)uaddr2;
    (void)val3;

    if (!uaddr)
        return -LINUX_EINVAL;

    switch (op & 0xF) {
        case FUTEX_WAIT:
            // If value doesn't match, return immediately
            if (*uaddr != val)
                return -LINUX_EAGAIN;

            if (timeout && timeout->tv_sec == 0 && timeout->tv_nsec == 0)
                return -LINUX_ETIMEDOUT;

            // No kernel scheduler wait queue integration yet.
            return -LINUX_EINTR;

        case FUTEX_WAKE:
            // pretend we woke threads
            return 1;

        default:
            return -LINUX_ENOSYS;
    }
}

struct passwd {
    char *pw_name;
    int pw_uid;
};

struct passwd fake_root = {
    .pw_name = "root",
    .pw_uid = 0,
};
