#include <multitasking.h>

#include <graphics.h>
#include <heap.h>
#include <memory.h>
#include <strings.h>
#include <userland.h>
#include <flanterm/flanterm.h>

typedef struct task {
    uint32_t pid;
    task_type_t type;
    task_state_t state;
    int exit_code;
    uint64_t created_at_tick;
    uint64_t runtime_ticks;
    char name[64];

    kernel_task_fn_t kernel_fn;
    void* kernel_ctx;

    user_task_spec_t user_spec;

    struct task* next;
} task_t;

extern struct flanterm_context* ft_ctx;

static task_t* g_task_head = NULL;
static task_t* g_task_tail = NULL;
static uint32_t g_next_pid = 1;
static uint32_t g_current_pid = 0;
static uint64_t g_last_tick = 0;

static uint64_t irq_save_disable(void) {
    uint64_t flags = 0;
    asm volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static void irq_restore(uint64_t flags) {
    asm volatile("pushq %0; popfq" :: "r"(flags) : "memory", "cc");
}

static task_t* find_task_locked(uint32_t pid) {
    for (task_t* task = g_task_head; task != NULL; task = task->next) {
        if (task->pid == pid)
            return task;
    }

    return NULL;
}

static void push_task_locked(task_t* task) {
    if (g_task_tail == NULL) {
        g_task_head = g_task_tail = task;
        return;
    }

    g_task_tail->next = task;
    g_task_tail = task;
}

static void fill_info(const task_t* task, task_info_t* info) {
    if (!task || !info)
        return;

    info->pid = task->pid;
    info->type = task->type;
    info->state = task->state;
    info->exit_code = task->exit_code;
    info->runtime_ticks = task->runtime_ticks;
    info->name = task->name;
}

void multitasking_init(void) {
    uint64_t flags = irq_save_disable();
    g_task_head = NULL;
    g_task_tail = NULL;
    g_next_pid = 1;
    g_current_pid = 0;
    g_last_tick = 0;
    irq_restore(flags);
}

uint32_t multitasking_current_pid(void) {
    uint64_t flags = irq_save_disable();
    uint32_t pid = g_current_pid;
    irq_restore(flags);
    return pid;
}

uint32_t multitasking_spawn_kernel(const char* name, kernel_task_fn_t fn, void* ctx) {
    if (!fn)
        return 0;

    task_t* task = (task_t*)kmalloc(sizeof(task_t));
    if (!task)
        return 0;

    memset(task, 0, sizeof(*task));

    uint64_t flags = irq_save_disable();
    task->pid = g_next_pid++;
    task->type = TASK_TYPE_KERNEL;
    task->state = TASK_STATE_READY;
    task->exit_code = 0;
    task->created_at_tick = g_last_tick;
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

uint32_t multitasking_spawn_userland(const char* name, const user_task_spec_t* spec) {
    if (!spec || !spec->path)
        return 0;

    task_t* task = (task_t*)kmalloc(sizeof(task_t));
    if (!task)
        return 0;

    memset(task, 0, sizeof(*task));

    uint64_t flags = irq_save_disable();
    task->pid = g_next_pid++;
    task->type = TASK_TYPE_USERLAND;
    task->state = TASK_STATE_READY;
    task->exit_code = 0;
    task->created_at_tick = g_last_tick;
    if (name)
        snprintf(task->name, sizeof(task->name), "%s", name);
    else
        snprintf(task->name, sizeof(task->name), "%s", spec->path);

    task->user_spec.path = spec->path;
    task->user_spec.argc = spec->argc;
    for (int i = 0; i < spec->argc && i < 32; ++i)
        task->user_spec.argv[i] = spec->argv[i];

    push_task_locked(task);
    irq_restore(flags);

    return task->pid;
}

bool multitasking_exit_task(uint32_t pid, int exit_code) {
    uint64_t flags = irq_save_disable();
    task_t* task = find_task_locked(pid);
    if (!task) {
        irq_restore(flags);
        return false;
    }

    task->state = TASK_STATE_EXITED;
    task->exit_code = exit_code;
    irq_restore(flags);

    return true;
}

bool multitasking_get_task(uint32_t pid, task_info_t* out_info) {
    if (!out_info)
        return false;

    uint64_t flags = irq_save_disable();
    task_t* task = find_task_locked(pid);
    if (!task) {
        irq_restore(flags);
        return false;
    }

    fill_info(task, out_info);
    irq_restore(flags);
    return true;
}

uint32_t multitasking_count_tasks(void) {
    uint32_t count = 0;

    uint64_t flags = irq_save_disable();
    for (task_t* task = g_task_head; task != NULL; task = task->next)
        count++;
    irq_restore(flags);

    return count;
}

uint32_t multitasking_count_running(void) {
    uint32_t count = 0;

    uint64_t flags = irq_save_disable();
    for (task_t* task = g_task_head; task != NULL; task = task->next) {
        if (task->state != TASK_STATE_EXITED)
            count++;
    }
    irq_restore(flags);

    return count;
}

bool multitasking_for_each_task(task_iter_cb_t cb, void* ctx) {
    if (!cb)
        return false;

    uint64_t flags = irq_save_disable();
    task_t* current = g_task_head;

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

    task_t* prev = NULL;
    task_t* cur = g_task_head;

    while (cur != NULL) {
        if (cur->state == TASK_STATE_EXITED) {
            task_t* dead = cur;
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

static bool cursor_blink_task(uint32_t pid, uint64_t now_ticks, void* ctx, int* exit_code) {
    (void)pid;
    (void)exit_code;

    if (!ft_ctx)
        return false;

    uint64_t* next_toggle_tick = (uint64_t*)ctx;
    if (!next_toggle_tick)
        return false;

    while (now_ticks >= *next_toggle_tick) {
        ft_ctx->cursor_enabled = !ft_ctx->cursor_enabled;
        *next_toggle_tick += 50;
    }
    return false;
}

void multitasking_start_cursor_blink_task(void) {
    uint64_t* blink_ctx = (uint64_t*)kmalloc(sizeof(uint64_t));
    if (!blink_ctx)
        return;

    *blink_ctx = 0;
    if (multitasking_spawn_kernel("cursor-blink", cursor_blink_task, blink_ctx) == 0)
        kfree(blink_ctx);
}

void multitasking_on_pit_tick(uint64_t now_ticks) {
    uint64_t flags = irq_save_disable();
    g_last_tick = now_ticks;

    for (task_t* task = g_task_head; task != NULL; task = task->next) {
        if (task->type != TASK_TYPE_KERNEL || task->state == TASK_STATE_EXITED)
            continue;

        task->state = TASK_STATE_RUNNING;
        g_current_pid = task->pid;
        kernel_task_fn_t fn = task->kernel_fn;
        void* kctx = task->kernel_ctx;

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
    task_t* run_task = NULL;

    uint64_t flags = irq_save_disable();
    for (task_t* task = g_task_head; task != NULL; task = task->next) {
        if (task->type == TASK_TYPE_USERLAND && task->state == TASK_STATE_READY) {
            task->state = TASK_STATE_RUNNING;
            g_current_pid = task->pid;
            run_task = task;
            break;
        }
    }
    irq_restore(flags);

    if (run_task) {
        int rc = userland_exec(run_task->user_spec.path,
                               run_task->user_spec.argc,
                               run_task->user_spec.argv,
                               NULL);

        flags = irq_save_disable();
        run_task->runtime_ticks += (g_last_tick - run_task->created_at_tick);
        run_task->state = TASK_STATE_EXITED;
        run_task->exit_code = rc;
        g_current_pid = 0;
        irq_restore(flags);
    }

    sweep_exited_tasks();
}
