#include <flanterm/flanterm.h>
#include <graphics.h>
#include <heap.h>
#include <memory.h>
#include <multitasking.h>
#include <strings.h>
#include <userland.h>
#include <debugger.h>

extern struct flanterm_context *ft_ctx;

static task_t *g_task_head = NULL;
static task_t *g_task_tail = NULL;
static uint32_t g_next_pid = 1;
static uint32_t g_current_pid = 0;
static uint64_t g_last_tick = 0;

static uint64_t irq_save_disable(void) {
    uint64_t flags = 0;
    asm volatile("pushfq; popq %0; cli" : "=r"(flags)::"memory");
    return flags;
}

static void irq_restore(uint64_t flags) {
    asm volatile("pushq %0; popfq" ::"r"(flags) : "memory", "cc");
}

static task_t *find_task_locked(uint32_t pid) {
    for (task_t *task = g_task_head; task != NULL; task = task->next) {
        if (task->pid == pid)
            return task;
    }

    return NULL;
}

task_t *multitasking_find_task(uint32_t pid) {
    return find_task_locked(pid);
}

task_t *multitasking_get_current_task(void) {
    uint32_t pid = multitasking_current_pid();
    if (!pid) {
        debug_printf("\t[multitasking] invalid pid.\n");
        return NULL;
    }

    for (task_t *t = g_task_head; t; t = t->next)
        if (t->pid == pid)
            return t;

    debug_printf("\t[multitasking] concluded failure.\n");
    return NULL;
}

static void push_task_locked(task_t *task) {
    if (g_task_tail == NULL) {
        g_task_head = g_task_tail = task;
        return;
    }

    g_task_tail->next = task;
    g_task_tail = task;
}


static void free_task(task_t *task) {
    if (!task)
        return;

    for (int i = 0; i < 32; i++) {
        if (task->user_spec.argv[i])
            kfree((void *)task->user_spec.argv[i]);
    }

    if (task->user_spec.path)
        kfree((void *)task->user_spec.path);

    if (task->kernel_ctx && strcmp(task->name, "cursor-blink") == 0)
        kfree(task->kernel_ctx);

    kfree(task);
}

static void fill_info(const task_t *task, task_info_t *info) {
    if (!task || !info)
        return;

    info->pid = task->pid;
    info->type = task->type;
    info->state = task->state;
    info->exit_code = task->exit_code;
    info->runtime_ticks = task->runtime_ticks;
    info->wakeup_tick = task->wakeup_tick;
    info->parent_pid = task->parent_pid;
    info->name = task->name;
    info->exe_path = task->user_spec.path;
}

void multitasking_init(void) {
    LOG_SCOPE();
    info("Initializing multitasking", __FILE__);
    uint64_t flags = irq_save_disable();
    g_task_head = NULL;
    g_task_tail = NULL;
    g_next_pid = 1;
    g_current_pid = 0;
    g_last_tick = 0;
    irq_restore(flags);
    done("Initialized multitasking", __FILE__);
}

uint32_t multitasking_current_pid(void) {
    uint64_t flags = irq_save_disable();
    uint32_t pid = g_current_pid;
    irq_restore(flags);
    return pid;
}

uint32_t multitasking_spawn_kernel(const char *name, kernel_task_fn_t fn, void *ctx) {
    if (!fn)
        return 0;

    task_t *task = (task_t *)kmalloc(sizeof(task_t));
    if (!task)
        return 0;

    memset(task, 0, sizeof(*task));

    uint64_t flags = irq_save_disable();
    task->pid = g_next_pid++;
    task->type = TASK_TYPE_KERNEL;
    task->state = TASK_STATE_READY;
    task->exit_code = 0;
    task->created_at_tick = g_last_tick;
    task->parent_pid = g_current_pid;
    task->kernel_fn = fn;
    task->kernel_ctx = ctx;
    if (name)
        snprintf(task->name, sizeof(task->name), "%s", name);
    else
        snprintf(task->name, sizeof(task->name), "kernel-task-%u", task->pid);

    push_task_locked(task);
    irq_restore(flags);

    return task->pid;
}

uint32_t multitasking_spawn_userland(const char *name, const user_task_spec_t *spec) {
    if (!spec || !spec->path)
        return 0;

    task_t *task = (task_t *)kmalloc(sizeof(task_t));
    if (!task)
        return 0;

    memset(task, 0, sizeof(*task));

    // ---------------------------
    // Basic task initialization
    // ---------------------------
    uint64_t flags = irq_save_disable();

    task->pid = g_next_pid++;
    task->type = TASK_TYPE_USERLAND;
    task->state = TASK_STATE_READY;
    task->exit_code = 0;
    task->created_at_tick = g_last_tick;
    task->parent_pid = spec->parent_pid ? spec->parent_pid : g_current_pid;
    task->fork_child = spec->fork_child;

    if (name)
        snprintf(task->name, sizeof(task->name), "%s", name);
    else
        snprintf(task->name, sizeof(task->name), "%s", spec->path);

    irq_restore(flags);

    // ---------------------------
    // Deep copy path
    // ---------------------------
    task->user_spec.path = strdup(spec->path);
    if (!task->user_spec.path)
        goto fail;

    // ---------------------------
    // Safe argc handling
    // ---------------------------
    int argc = spec->argc;
    if (argc < 0)
        argc = 0;
    if (argc > 31)
        argc = 31;

    task->user_spec.argc = argc;
    task->user_spec.parent_pid = task->parent_pid;
    task->user_spec.fork_child = task->fork_child;

    // ---------------------------
    // Deep copy argv safely
    // ---------------------------
    for (int i = 0; i < argc; i++) {
        if (spec->argv[i]) {
            task->user_spec.argv[i] = strdup(spec->argv[i]);
            if (!task->user_spec.argv[i])
                goto fail;
        } else {
            task->user_spec.argv[i] = NULL;
        }
    }

    task->user_spec.argv[argc] = NULL;

    // optional safety padding (prevents garbage reads)
    for (int i = argc + 1; i < 32; i++)
        task->user_spec.argv[i] = NULL;

    // ---------------------------
    // Queue task
    // ---------------------------
    flags = irq_save_disable();
    push_task_locked(task);
    irq_restore(flags);

    return task->pid;

fail:
    // ---------------------------
    // Cleanup on partial failure
    // ---------------------------
    free_task(task);
    return 0;
}

bool multitasking_exit_task(uint32_t pid, int exit_code) {
    uint64_t flags = irq_save_disable();
    task_t *task = find_task_locked(pid);
    if (!task) {
        irq_restore(flags);
        return false;
    }

    if (task->state == TASK_STATE_EXITED) {
        irq_restore(flags);
        return true;
    }

    task->state = TASK_STATE_EXITED;
    task->exit_code = exit_code;
    for (task_t *child = g_task_head; child != NULL; child = child->next) {
        if (child->parent_pid == pid)
            child->parent_pid = 1;
    }
    irq_restore(flags);

    return true;
}

bool multitasking_kill_task(uint32_t pid, int signal) {
    if (pid == 0 || signal < 0)
        return false;
    return multitasking_exit_task(pid, 128 + signal);
}

bool multitasking_sleep_task(uint32_t pid, uint64_t wakeup_tick) {
    uint64_t flags = irq_save_disable();
    task_t *task = find_task_locked(pid);
    if (!task || task->state == TASK_STATE_EXITED) {
        irq_restore(flags);
        return false;
    }
    task->wakeup_tick = wakeup_tick;
    task->state = TASK_STATE_SLEEPING;
    irq_restore(flags);
    return true;
}

void multitasking_yield(void) {
    multitasking_pump();
}

bool multitasking_update_user_image(uint32_t pid, const char *path, int argc, const char *const argv[]) {
    if (!path)
        return false;

    if (argc < 0)
        argc = 0;
    if (argc > 31)
        argc = 31;

    char *new_path = strdup(path);
    if (!new_path)
        return false;

    const char *new_argv[32];
    memset(new_argv, 0, sizeof(new_argv));

    for (int i = 0; i < argc; i++) {
        const char *arg = (argv && argv[i]) ? argv[i] : "";
        new_argv[i] = strdup(arg);
        if (!new_argv[i]) {
            for (int j = 0; j < i; j++)
                kfree((void *)new_argv[j]);
            kfree(new_path);
            return false;
        }
    }

    uint64_t flags = irq_save_disable();
    task_t *task = find_task_locked(pid);
    if (!task || task->type != TASK_TYPE_USERLAND || task->state == TASK_STATE_EXITED) {
        irq_restore(flags);
        for (int i = 0; i < argc; i++)
            kfree((void *)new_argv[i]);
        kfree(new_path);
        return false;
    }

    const char *old_path = task->user_spec.path;
    const char *old_argv[32];
    for (int i = 0; i < 32; i++)
        old_argv[i] = task->user_spec.argv[i];

    task->user_spec.path = new_path;
    task->user_spec.argc = argc;
    for (int i = 0; i < 32; i++)
        task->user_spec.argv[i] = (i < argc) ? new_argv[i] : NULL;
    snprintf(task->name, sizeof(task->name), "%s", path);

    irq_restore(flags);

    for (int i = 0; i < 32; i++) {
        if (old_argv[i])
            kfree((void *)old_argv[i]);
    }
    if (old_path)
        kfree((void *)old_path);

    return true;
}

bool multitasking_get_task(uint32_t pid, task_info_t *out_info) {
    if (!out_info)
        return false;

    uint64_t flags = irq_save_disable();
    task_t *task = find_task_locked(pid);
    if (!task) {
        irq_restore(flags);
        return false;
    }

    fill_info(task, out_info);
    irq_restore(flags);
    return true;
}

static bool child_matches_filter(const task_t *task, uint32_t parent_pid, int64_t pid_filter) {
    if (!task || task->parent_pid != parent_pid)
        return false;

    if (pid_filter == -1)
        return true;

    if (pid_filter > 0)
        return task->pid == (uint32_t)pid_filter;

    /* Process groups are not modelled yet; pid 0 and pid < -1 mean any child. */
    return true;
}

bool multitasking_find_child(uint32_t parent_pid, int64_t pid_filter, bool exited_only, task_info_t *out_info) {
    uint64_t flags = irq_save_disable();

    for (task_t *task = g_task_head; task != NULL; task = task->next) {
        if (!child_matches_filter(task, parent_pid, pid_filter))
            continue;
        if (exited_only && task->state != TASK_STATE_EXITED)
            continue;

        if (out_info)
            fill_info(task, out_info);
        irq_restore(flags);
        return true;
    }

    irq_restore(flags);
    return false;
}

bool multitasking_reap_task(uint32_t pid, task_info_t *out_info) {
    uint64_t flags = irq_save_disable();
    task_t *prev = NULL;
    task_t *task = g_task_head;

    while (task) {
        if (task->pid == pid)
            break;
        prev = task;
        task = task->next;
    }

    if (!task || task->state != TASK_STATE_EXITED) {
        irq_restore(flags);
        return false;
    }

    if (out_info)
        fill_info(task, out_info);

    if (prev)
        prev->next = task->next;
    else
        g_task_head = task->next;

    if (task == g_task_tail)
        g_task_tail = prev;

    irq_restore(flags);
    free_task(task);
    return true;
}

bool multitasking_current_is_fork_child(void) {
    uint64_t flags = irq_save_disable();
    task_t *task = find_task_locked(g_current_pid);
    bool is_child = task && task->fork_child;
    if (task)
        task->fork_child = false;
    irq_restore(flags);
    return is_child;
}

uint32_t multitasking_count_tasks(void) {
    uint32_t count = 0;

    uint64_t flags = irq_save_disable();
    for (task_t *task = g_task_head; task != NULL; task = task->next)
        count++;
    irq_restore(flags);

    return count;
}

uint32_t multitasking_count_running(void) {
    uint32_t count = 0;

    uint64_t flags = irq_save_disable();
    for (task_t *task = g_task_head; task != NULL; task = task->next) {
        if (task->state != TASK_STATE_EXITED)
            count++;
    }
    irq_restore(flags);

    return count;
}

bool multitasking_for_each_task(task_iter_cb_t cb, void *ctx) {
    if (!cb)
        return false;

    uint64_t flags = irq_save_disable();
    task_t *current = g_task_head;

    while (current != NULL) {
        task_info_t info;
        fill_info(current, &info);

        irq_restore(flags);
        bool keep = cb(&info, ctx);
        if (!keep)
            return true;

        flags = irq_save_disable();
        current = current->next;
    }

    irq_restore(flags);
    return true;
}

static void sweep_exited_tasks(void) {
    uint64_t flags = irq_save_disable();

    task_t *prev = NULL;
    task_t *cur = g_task_head;

    while (cur != NULL) {
        if (cur->state == TASK_STATE_EXITED) {
            task_t *dead = cur;
            cur = cur->next;

            if (prev)
                prev->next = cur;
            else
                g_task_head = cur;

            if (dead == g_task_tail)
                g_task_tail = prev;

            irq_restore(flags);
            kfree(dead);
            flags = irq_save_disable();
            continue;
        }

        prev = cur;
        cur = cur->next;
    }

    irq_restore(flags);
}

static bool cursor_blink_task(uint32_t pid, uint64_t now_ticks, void *ctx, int *exit_code) {
    (void)pid;
    (void)exit_code;

    if (!ft_ctx)
        return false;

    uint64_t *next_toggle_tick = (uint64_t *)ctx;
    if (!next_toggle_tick)
        return false;

    while (now_ticks >= *next_toggle_tick) {
        ft_ctx->cursor_enabled = !ft_ctx->cursor_enabled;
        *next_toggle_tick += 50;
    }
    return false;
}

void multitasking_start_cursor_blink_task(void) {
    uint64_t *blink_ctx = (uint64_t *)kmalloc(sizeof(uint64_t));
    if (!blink_ctx)
        return;

    *blink_ctx = 0;
    if (multitasking_spawn_kernel("cursor-blink", cursor_blink_task, blink_ctx) == 0)
        kfree(blink_ctx);
}

void multitasking_on_pit_tick(uint64_t now_ticks) {
    uint64_t flags = irq_save_disable();
    g_last_tick = now_ticks;

    for (task_t *task = g_task_head; task != NULL; task = task->next) {
        if (task->state == TASK_STATE_SLEEPING && task->wakeup_tick <= now_ticks)
            task->state = TASK_STATE_READY;
        if (task->type != TASK_TYPE_KERNEL || task->state != TASK_STATE_READY)
            continue;

        task->state = TASK_STATE_RUNNING;
        g_current_pid = task->pid;
        kernel_task_fn_t fn = task->kernel_fn;
        void *kctx = task->kernel_ctx;

        irq_restore(flags);

        int exit_code = 0;
        bool should_exit = fn(task->pid, now_ticks, kctx, &exit_code);

        flags = irq_save_disable();

        task->runtime_ticks++;
        if (should_exit) {
            task->state = TASK_STATE_EXITED;
            task->exit_code = exit_code;
        } else {
            task->state = TASK_STATE_READY;
        }
    }

    g_current_pid = 0;
    irq_restore(flags);
}

void multitasking_pump(void) {
    task_t *task_to_run = NULL;

    uint64_t flags = irq_save_disable();

    for (task_t *task = g_task_head; task; task = task->next) {
        if (task->state == TASK_STATE_SLEEPING && task->wakeup_tick <= g_last_tick)
            task->state = TASK_STATE_READY;
        if (task->type == TASK_TYPE_USERLAND &&
            task->state == TASK_STATE_READY) {
            task->state = TASK_STATE_RUNNING;
            g_current_pid = task->pid;
            task_to_run = task;
            break;
        }
    }

    irq_restore(flags);

    if (!task_to_run)
        return;

    // ---------------------------
    // FIRST TIME RUN ONLY
    // ---------------------------
    if (!task_to_run->user_runtime.started) {
        userland_exec_ctx_t ctx;

        ctx.path = task_to_run->user_spec.path;

        int argc = task_to_run->user_spec.argc;
        if (argc < 0)
            argc = 0;
        if (argc > 31)
            argc = 31;

        ctx.argc = argc;
        ctx.envp = NULL;

        for (int i = 0; i < argc; i++)
            ctx.argv[i] = task_to_run->user_spec.argv[i];

        ctx.argv[argc] = NULL;

        // run ELF ONCE
        int rc = userland_exec(&ctx);

        task_to_run->state = TASK_STATE_EXITED;
        task_to_run->exit_code = rc;
        task_to_run->user_runtime.started = 1;

        g_current_pid = 0;
        return;
    }

    // ---------------------------
    // AFTER FIRST RUN: SHOULD NEVER RELOAD ELF
    // ---------------------------
    task_to_run->state = TASK_STATE_EXITED;
    task_to_run->exit_code = -LINUX_ENOSYS;

    g_current_pid = 0;
}