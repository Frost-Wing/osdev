#include <debugger.h>
#include <graphics.h>
#include <heap.h>
#include <multitasking.h>
#include <strings.h>
#include <tty.h>
#include <userland.h>
#include <memory.h>

#define ARGV_MAX 32

static task_t *g_task_head;
static task_t *g_task_tail;
static uint32_t g_next_pid = 1;
static uint32_t g_current_pid;
static uint64_t g_last_tick;

static inline uint64_t irq_save_disable(void) {
    uint64_t flags;
    asm volatile("pushfq; popq %0; cli" : "=r"(flags)::"memory");
    return flags;
}

static inline void irq_restore(uint64_t flags) {
    asm volatile("pushq %0; popfq" ::"r"(flags) : "memory", "cc");
}

static inline void irq_guard_release(uint64_t *flags) {
    irq_restore(*flags);
}

#define IRQ_GUARD() uint64_t irq_guard_ __attribute__((cleanup(irq_guard_release))) = irq_save_disable()

static void kfree_const(const void *p) {
    if (p)
        kfree((void *)p);
}

/* The list is sorted by pid: pids are assigned at push time under the lock. */
static task_t *find_task_locked(uint32_t pid) {
    for (task_t *t = g_task_head; t && t->pid <= pid; t = t->next) {
        if (t->pid == pid)
            return t;
    }
    return NULL;
}

static task_t *first_task_after(uint32_t pid) {
    task_t *t = g_task_head;
    while (t && t->pid <= pid)
        t = t->next;
    return t;
}

static uint32_t push_task_locked(task_t *task) {
    task->pid = g_next_pid++;
    task->next = NULL;
    if (g_task_tail)
        g_task_tail->next = task;
    else
        g_task_head = task;
    g_task_tail = task;
    return task->pid;
}

static void unlink_task_locked(task_t *prev, task_t *task) {
    if (prev)
        prev->next = task->next;
    else
        g_task_head = task->next;
    if (task == g_task_tail)
        g_task_tail = prev;
}

static void free_user_spec(user_task_spec_t *spec) {
    for (int i = 0; i < ARGV_MAX; i++)
        kfree_const(spec->argv[i]);
    kfree_const(spec->path);
    spec->path = NULL;
    memset(spec->argv, 0, sizeof(spec->argv));
}

static bool copy_user_spec(user_task_spec_t *dst, const char *path, int argc, const char *const *argv) {
    memset(dst, 0, sizeof(*dst));

    dst->path = strdup(path);
    if (!dst->path)
        return false;

    if (argc < 0)
        argc = 0;
    if (argc > ARGV_MAX - 1)
        argc = ARGV_MAX - 1;
    dst->argc = argc;

    for (int i = 0; i < argc; i++) {
        dst->argv[i] = strdup(argv && argv[i] ? argv[i] : "");
        if (!dst->argv[i]) {
            free_user_spec(dst);
            return false;
        }
    }
    return true;
}

static void free_task(task_t *task) {
    if (!task)
        return;
    free_user_spec(&task->user_spec);
    if (task->owns_kernel_ctx)
        kfree(task->kernel_ctx);
    kfree(task);
}

static void fill_info(const task_t *task, task_info_t *info) {
    info->pid = task->pid;
    info->type = task->type;
    info->state = task->state;
    info->exit_code = task->exit_code;
    info->runtime_ticks = task->runtime_ticks;
    info->wakeup_tick = task->wakeup_tick;
    info->parent_pid = task->parent_pid;
    info->tty_index = task->tty_index;
    snprintf(info->name, sizeof(info->name), "%s", task->name);
    snprintf(info->exe_path, sizeof(info->exe_path), "%s", task->user_spec.path ? task->user_spec.path : "");
}

void multitasking_init(void) {
    LOG_SCOPE();
    info("Initializing multitasking", __FILE__);
    {
        IRQ_GUARD();
        g_task_head = g_task_tail = NULL;
        g_next_pid = 1;
        g_current_pid = 0;
        g_last_tick = 0;
    }
    done("Initialized multitasking", __FILE__);
}

task_t *multitasking_find_task(uint32_t pid) {
    IRQ_GUARD();
    return find_task_locked(pid);
}

task_t *multitasking_get_current_task(void) {
    IRQ_GUARD();
    return find_task_locked(g_current_pid);
}

uint32_t multitasking_current_pid(void) {
    return g_current_pid;
}

uint8_t multitasking_current_tty(void) {
    IRQ_GUARD();
    task_t *task = find_task_locked(g_current_pid);
    return task ? task->tty_index : tty_active_index();
}

static uint32_t spawn_kernel(const char *name, kernel_task_fn_t fn, void *ctx, bool owns_ctx) {
    if (!fn)
        return 0;

    task_t *task = kmalloc(sizeof(*task));
    if (!task)
        return 0;
    memset(task, 0, sizeof(*task));

    task->type = TASK_TYPE_KERNEL;
    task->state = TASK_STATE_READY;
    task->kernel_fn = fn;
    task->kernel_ctx = ctx;
    task->owns_kernel_ctx = owns_ctx;

    IRQ_GUARD();
    task->created_at_tick = g_last_tick;
    task->parent_pid = g_current_pid;
    task->tty_index = multitasking_current_tty();

    uint32_t pid = push_task_locked(task);
    if (name)
        snprintf(task->name, sizeof(task->name), "%s", name);
    else
        snprintf(task->name, sizeof(task->name), "kernel-task-%u", pid);
    return pid;
}

uint32_t multitasking_spawn_kernel(const char *name, kernel_task_fn_t fn, void *ctx) {
    return spawn_kernel(name, fn, ctx, false);
}

uint32_t multitasking_spawn_kernel_owned(const char *name, kernel_task_fn_t fn, void *ctx) {
    return spawn_kernel(name, fn, ctx, true);
}

uint32_t multitasking_spawn_userland(const char *name, const user_task_spec_t *spec) {
    if (!spec || !spec->path)
        return 0;

    task_t *task = kmalloc(sizeof(*task));
    if (!task)
        return 0;
    memset(task, 0, sizeof(*task));

    if (!copy_user_spec(&task->user_spec, spec->path, spec->argc, spec->argv)) {
        kfree(task);
        return 0;
    }

    task->type = TASK_TYPE_USERLAND;
    task->state = TASK_STATE_READY;
    task->fork_child = spec->fork_child;
    task->user_spec.fork_child = spec->fork_child;
    snprintf(task->name, sizeof(task->name), "%s", name ? name : spec->path);

    IRQ_GUARD();
    task->created_at_tick = g_last_tick;
    task->parent_pid = spec->parent_pid ? spec->parent_pid : g_current_pid;
    task->user_spec.parent_pid = task->parent_pid;
    task->tty_index = spec->tty_index < TTY_COUNT ? spec->tty_index : multitasking_current_tty();

    return push_task_locked(task);
}

bool multitasking_exit_task(uint32_t pid, int exit_code) {
    IRQ_GUARD();
    task_t *task = find_task_locked(pid);
    if (!task)
        return false;
    if (task->state == TASK_STATE_EXITED)
        return true;

    task->state = TASK_STATE_EXITED;
    task->exit_code = exit_code;
    for (task_t *t = g_task_head; t; t = t->next) {
        if (t->parent_pid == pid)
            t->parent_pid = 1;
    }
    return true;
}

bool multitasking_kill_task(uint32_t pid, int signal) {
    if (pid == 0 || signal < 0)
        return false;
    if (signal == 0) {
        IRQ_GUARD();
        return find_task_locked(pid) != NULL;
    }
    return multitasking_exit_task(pid, 128 + signal);
}

bool multitasking_sleep_task(uint32_t pid, uint64_t wakeup_tick) {
    IRQ_GUARD();
    task_t *task = find_task_locked(pid);
    if (!task || task->state == TASK_STATE_EXITED)
        return false;
    task->wakeup_tick = wakeup_tick;
    task->state = TASK_STATE_SLEEPING;
    return true;
}

void multitasking_yield(void) {
    multitasking_pump();
}

bool multitasking_update_user_image(uint32_t pid, const char *path, int argc, const char *const argv[]) {
    if (!path)
        return false;

    user_task_spec_t fresh;
    if (!copy_user_spec(&fresh, path, argc, argv))
        return false;

    user_task_spec_t old;
    bool ok = false;
    {
        IRQ_GUARD();
        task_t *task = find_task_locked(pid);
        if (task && task->type == TASK_TYPE_USERLAND && task->state != TASK_STATE_EXITED) {
            old = task->user_spec;
            task->user_spec.path = fresh.path;
            task->user_spec.argc = fresh.argc;
            memcpy(task->user_spec.argv, fresh.argv, sizeof(fresh.argv));
            snprintf(task->name, sizeof(task->name), "%s", path);
            ok = true;
        }
    }

    free_user_spec(ok ? &old : &fresh);
    return ok;
}

bool multitasking_get_task(uint32_t pid, task_info_t *out_info) {
    if (!out_info)
        return false;

    IRQ_GUARD();
    task_t *task = find_task_locked(pid);
    if (!task)
        return false;
    fill_info(task, out_info);
    return true;
}

static bool child_matches_filter(const task_t *task, uint32_t parent_pid, int64_t pid_filter) {
    if (task->parent_pid != parent_pid)
        return false;
    if (pid_filter > 0)
        return task->pid == (uint32_t)pid_filter;
    /* Process groups are not modelled yet; pid 0 and pid < -1 mean any child. */
    return true;
}

bool multitasking_find_child(uint32_t parent_pid, int64_t pid_filter, bool exited_only, task_info_t *out_info) {
    IRQ_GUARD();
    for (task_t *task = g_task_head; task; task = task->next) {
        if (!child_matches_filter(task, parent_pid, pid_filter))
            continue;
        if (exited_only && task->state != TASK_STATE_EXITED)
            continue;
        if (out_info)
            fill_info(task, out_info);
        return true;
    }
    return false;
}

bool multitasking_reap_task(uint32_t pid, task_info_t *out_info) {
    task_t *task;
    {
        IRQ_GUARD();
        task_t *prev = NULL;
        task = g_task_head;
        while (task && task->pid != pid) {
            prev = task;
            task = task->next;
        }
        if (!task || task->state != TASK_STATE_EXITED)
            return false;

        if (out_info)
            fill_info(task, out_info);
        unlink_task_locked(prev, task);
    }
    free_task(task);
    return true;
}

bool multitasking_current_is_fork_child(void) {
    IRQ_GUARD();
    task_t *task = find_task_locked(g_current_pid);
    if (!task)
        return false;
    bool is_child = task->fork_child;
    task->fork_child = false;
    return is_child;
}

uint32_t multitasking_count_tasks(void) {
    uint32_t count = 0;
    IRQ_GUARD();
    for (task_t *t = g_task_head; t; t = t->next)
        count++;
    return count;
}

uint32_t multitasking_count_running(void) {
    uint32_t count = 0;
    IRQ_GUARD();
    for (task_t *t = g_task_head; t; t = t->next)
        count += t->state != TASK_STATE_EXITED;
    return count;
}

/* Iterates by pid cursor so the callback may reap tasks, including the current one. */
bool multitasking_for_each_task(task_iter_cb_t cb, void *ctx) {
    if (!cb)
        return false;

    uint32_t last = 0;
    for (;;) {
        task_info_t info;
        {
            IRQ_GUARD();
            task_t *task = first_task_after(last);
            if (!task)
                return true;
            last = task->pid;
            fill_info(task, &info);
        }
        if (!cb(&info, ctx))
            return true;
    }
}

void multitasking_on_pit_tick(uint64_t now_ticks) {
    uint64_t flags = irq_save_disable();
    uint32_t saved_pid = g_current_pid;
    g_last_tick = now_ticks;

    task_t *task = g_task_head;
    while (task) {
        uint32_t pid = task->pid;

        if (task->state == TASK_STATE_SLEEPING && task->wakeup_tick <= now_ticks)
            task->state = TASK_STATE_READY;

        if (task->type != TASK_TYPE_KERNEL || task->state != TASK_STATE_READY) {
            task = task->next;
            continue;
        }

        task->state = TASK_STATE_RUNNING;
        g_current_pid = pid;
        kernel_task_fn_t fn = task->kernel_fn;
        void *kctx = task->kernel_ctx;

        irq_restore(flags);
        int exit_code = 0;
        bool should_exit = fn(pid, now_ticks, kctx, &exit_code);
        flags = irq_save_disable();

        /* fn may have run long enough for the task to be killed or reaped. */
        task = find_task_locked(pid);
        if (task) {
            task->runtime_ticks++;
            if (should_exit && task->state != TASK_STATE_EXITED) {
                task->state = TASK_STATE_EXITED;
                task->exit_code = exit_code;
            } else if (task->state == TASK_STATE_RUNNING) {
                task->state = TASK_STATE_READY;
            }
        }
        task = first_task_after(pid);
    }

    g_current_pid = saved_pid;
    irq_restore(flags);
}

void multitasking_pump(void) {
    userland_exec_ctx_t ctx;
    uint32_t pid;
    uint32_t saved_pid;

    {
        IRQ_GUARD();
        saved_pid = g_current_pid;

        task_t *task = NULL;
        for (task_t *t = g_task_head; t; t = t->next) {
            if (t->state == TASK_STATE_SLEEPING && t->wakeup_tick <= g_last_tick)
                t->state = TASK_STATE_READY;
            if (t->type == TASK_TYPE_USERLAND && t->state == TASK_STATE_READY && !t->user_runtime.started) {
                task = t;
                break;
            }
        }
        if (!task)
            return;

        pid = task->pid;
        task->state = TASK_STATE_RUNNING;
        task->user_runtime.started = 1;
        g_current_pid = pid;

        int argc = task->user_spec.argc;
        ctx.path = task->user_spec.path;
        ctx.argc = argc;
        ctx.envp = NULL;
        for (int i = 0; i < argc; i++)
            ctx.argv[i] = task->user_spec.argv[i];
        ctx.argv[argc] = NULL;
    }

    int rc = userland_exec(&ctx);

    IRQ_GUARD();
    task_t *task = find_task_locked(pid);
    if (task && task->state != TASK_STATE_EXITED) {
        task->state = TASK_STATE_EXITED;
        task->exit_code = rc;
    }
    g_current_pid = saved_pid;
}

// Multitasking testing ground below

static bool idle_fn(uint32_t pid, uint64_t now, void *ctx, int *exit_code) {
    (void)pid; (void)now; (void)ctx; (void)exit_code;
    return false;
}

static bool suicide_fn(uint32_t pid, uint64_t now, void *ctx, int *exit_code) {
    (void)now; (void)ctx; (void)exit_code;
    multitasking_kill_task(pid, 9);
    return false;
}

static bool reap_cb(const task_info_t *i, void *ctx) {
    (void)ctx;
    multitasking_exit_task(i->pid, 0);
    multitasking_reap_task(i->pid, NULL);
    return true;
}

void multitasking_selftest(void) {
    LOG_SCOPE();
    info("Multitasking self-test beginning", __FILE__);
    task_info_t ti;

    /* spawn / count */
    uint32_t a = multitasking_spawn_kernel("a", idle_fn, NULL);
    assert(a == 1);
    assert(multitasking_count_tasks() == 1);

    /* reaping a non-exited task must fail */
    assert(!multitasking_reap_task(a, NULL));

    /* signal 0 = existence check, must not kill */
    assert(multitasking_kill_task(a, 0));
    assert(multitasking_get_task(a, &ti));
    assert(ti.state != TASK_STATE_EXITED);

    /* kill -> exit code 128 + signal, info survives the free */
    assert(multitasking_kill_task(a, 9));
    assert(multitasking_reap_task(a, &ti));
    assert(ti.exit_code == 137);
    assert(strcmp(ti.name, "a") == 0);
    assert(multitasking_count_tasks() == 0);

    /* for_each with a callback that reaps the visited task */
    for (int i = 0; i < 100; i++)
        assert(multitasking_spawn_kernel("x", idle_fn, NULL) != 0);
    assert(multitasking_count_tasks() == 100);
    multitasking_for_each_task(reap_cb, NULL);
    assert(multitasking_count_tasks() == 0);

    /* a task that kills itself during its own fn must end up EXITED */
    uint32_t s = multitasking_spawn_kernel("s", suicide_fn, NULL);
    multitasking_on_pit_tick(1);
    assert(multitasking_get_task(s, &ti));
    assert(ti.state == TASK_STATE_EXITED);
    assert(ti.exit_code == 137);
    assert(multitasking_reap_task(s, NULL));

    /* sleep / wake: must not run before wakeup_tick */
    uint32_t z = multitasking_spawn_kernel("z", idle_fn, NULL);
    multitasking_sleep_task(z, 50);
    multitasking_on_pit_tick(10);
    multitasking_get_task(z, &ti);
    assert(ti.state == TASK_STATE_SLEEPING);
    assert(ti.runtime_ticks == 0);
    multitasking_on_pit_tick(50);
    multitasking_get_task(z, &ti);
    assert(ti.runtime_ticks == 1);
    multitasking_kill_task(z, 9);
    multitasking_reap_task(z, NULL);

    /* orphan reparenting: killing a parent moves its children to pid 1 */
    uint32_t p = multitasking_spawn_kernel("p", idle_fn, NULL);
    user_task_spec_t spec = { .path = "/bin/x", .argc = 0, .parent_pid = p, .tty_index = 0xFF };
    uint32_t c = multitasking_spawn_userland("c", &spec);
    assert(c != 0);
    multitasking_kill_task(p, 9);
    multitasking_get_task(c, &ti);
    assert(ti.parent_pid == 1);

    /* clean up */
    multitasking_reap_task(p, NULL);
    multitasking_kill_task(c, 9);
    multitasking_reap_task(c, NULL);

    done("Multitasking self-test was successful", __FILE__);
}