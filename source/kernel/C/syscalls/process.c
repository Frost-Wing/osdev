#include <syscalls/internal.h>

uint64 sys_rt_sigaction(int signum, const task_signal_action_t *action,
    task_signal_action_t *old_action, uint64_t sigsetsize) {
    if (sigsetsize != sizeof(uint64_t) ||
        signum < 1 || signum > 64 ||
        signum == SIGKILL || signum == SIGSTOP)
        return -LINUX_EINVAL;

    task_t *task = multitasking_get_current_task();
    if (!task)
        return -LINUX_EINVAL;

    if (old_action)
        *old_action = task->signal_actions[signum];

    if (action) {
        task_signal_action_t updated = *action;
        updated.mask &= ~((1ULL << (SIGKILL - 1)) | (1ULL << (SIGSTOP - 1)));
        task->signal_actions[signum] = updated;
    }
    return 0;
}

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

    if (rc == 0) {
        task_t *task = multitasking_get_current_task();
        if (task)
            task->membarrier_registered = 0;
    }

    return (rc == 0) ? 0 : -LINUX_ENOEXEC;
}

extern syscall_frame_t *current_syscall_frame;

typedef struct {
    uint64_t flags;
    uint64_t pidfd;
    uint64_t child_tid;
    uint64_t parent_tid;
    uint64_t exit_signal;
    uint64_t stack;
    uint64_t stack_size;
    uint64_t tls;
    uint64_t set_tid;
    uint64_t set_tid_size;
    uint64_t cgroup;
} linux_clone_args_t;

_Static_assert(sizeof(linux_clone_args_t) == 88, "clone3 ABI size");

uint64 sys_clone3(const void *args, uint64_t size) {
    const uint64_t LINUX_SIGCHLD = 17;
    const uint64_t CLONE_ARGS_MIN_SIZE = 64;

    if (!args)
        return -LINUX_EFAULT;
    if (size < CLONE_ARGS_MIN_SIZE)
        return -LINUX_EINVAL;
    if (size > sizeof(linux_clone_args_t))
        return -LINUX_E2BIG;

    linux_clone_args_t clone_args = {0};
    memcpy(&clone_args, args, size);

    if (clone_args.flags != 0 || clone_args.exit_signal != LINUX_SIGCHLD ||
        clone_args.pidfd || clone_args.child_tid || clone_args.parent_tid ||
        clone_args.stack || clone_args.stack_size || clone_args.tls ||
        clone_args.set_tid || clone_args.set_tid_size || clone_args.cgroup)
        return -LINUX_ENOSYS;

    return sys_fork();
}

uint64 sys_fork(void) {
    /* Read the frame BEFORE the child runs: the child's syscalls overwrite this global. */
    const syscall_frame_t *f = current_syscall_frame;
    task_t *cur = multitasking_get_current_task();
    if (!f || !cur || !cur->user_spec.path) {
        debug_printf("[syscall] fork: no frame/task/path\n");
        return -LINUX_ENOSYS;
    }

    uint32_t parent_pid = cur->pid;

    user_task_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.path = cur->user_spec.path;
    spec.argc = cur->user_spec.argc > 31 ? 31 : cur->user_spec.argc;
    for (int i = 0; i < spec.argc; i++)
        spec.argv[i] = cur->user_spec.argv[i] ? cur->user_spec.argv[i] : "";
    spec.parent_pid = parent_pid;
    spec.tty_index = cur->tty_index;
    spec.fork_child = true;              /* means "already running", see multitasking.c */

    uint32_t child = multitasking_spawn_userland(NULL, &spec);
    if (!child)
        return -LINUX_EAGAIN;
    task_t *child_task = multitasking_find_task(child);
    if (child_task)
        memcpy(child_task->signal_actions, cur->signal_actions,
            sizeof(child_task->signal_actions));

    /* Copy everything out of the frame now. */
    userland_regs_t regs = {
        .rax = 0,                        /* fork() returns 0 in the child */
        .rbx = f->rbx, .rcx = f->rip, .rdx = f->rdx,
        .rsi = f->rsi, .rdi = f->rdi, .rbp = f->rbp,
        .r8 = f->r8, .r9 = f->r9, .r10 = f->r10,
        .r11 = f->rflags,
        .r12 = f->r12, .r13 = f->r13, .r14 = f->r14, .r15 = f->r15,
        .rip = f->rip,
        .rflags = f->rflags | 0x202,
        .rsp = f->rsp,
    };

    multitasking_set_current_pid(child);     /* getpid()/tty/execve now see the child */
    int rc = userland_fork(&regs);           /* returns only when the child has exited */
    multitasking_set_current_pid(parent_pid);

    if (rc == USERLAND_FORK_FAILED) {
        multitasking_exit_task(child, 127);
        multitasking_reap_task(child, NULL);
        return -LINUX_EAGAIN;
    }

    multitasking_exit_task(child, rc);       /* no-op if the exit syscall already marked it */
    return child;
}

uint64 sys_wait4(int64_t pid, int *status, int options, void *rusage) {
    if (rusage)
        memset(rusage, 0, 128);
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
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP 5
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_CLOCK_REALTIME 256

uint64 sys_futex(uint32_t *uaddr, int op, uint32_t val,
    const linux_timespec_t *timeout,
    uint32_t *uaddr2, uint32_t val3) {
    if (!uaddr)
        return -LINUX_EINVAL;
    if ((op & ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME | 0x7F)) != 0)
        return -LINUX_EINVAL;

    switch (op & 0x7F) {
        case FUTEX_WAIT:
        case FUTEX_WAIT_BITSET:
            if ((op & 0x7F) == FUTEX_WAIT_BITSET && val3 == 0)
                return -LINUX_EINVAL;
            if (timeout &&
                (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
                    timeout->tv_nsec >= 1000000000L))
                return -LINUX_EINVAL;
            if (__atomic_load_n(uaddr, __ATOMIC_SEQ_CST) != val)
                return -LINUX_EAGAIN;
            if (timeout && timeout->tv_sec == 0 && timeout->tv_nsec == 0)
                return -LINUX_ETIMEDOUT;
            /*
             * No kernel wait queues exist, so report a retryable compare
             * miss rather than claiming a waiter was actually interrupted.
             */
            return -LINUX_EAGAIN;

        case FUTEX_WAKE:
        case FUTEX_WAKE_BITSET:
            if ((op & 0x7F) == FUTEX_WAKE_BITSET && val3 == 0)
                return -LINUX_EINVAL;
            return 0;

        case FUTEX_REQUEUE:
            return uaddr2 ? 0 : -LINUX_EINVAL;

        case FUTEX_CMP_REQUEUE:
            if (!uaddr2)
                return -LINUX_EINVAL;
            return __atomic_load_n(uaddr, __ATOMIC_SEQ_CST) == val3 ?
                0 : -LINUX_EAGAIN;

        case FUTEX_WAKE_OP:
            return uaddr2 ? -LINUX_ENOSYS : -LINUX_EINVAL;

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
