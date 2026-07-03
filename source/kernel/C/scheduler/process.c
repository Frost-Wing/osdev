#include <scheduler/process.h>
#include <memory.h>
#include <heap.h>
#include <paging.h>
#include <scheduler/scheduler.h>
#include <pit.h>

static process_t *process_list = NULL;
static uint32_t next_pid = 1;


/* -----------------------------
 * INTERNAL HELPERS
 * ----------------------------- */

static process_t *alloc_process(void)
{
    process_t *p = (process_t *)kmalloc(sizeof(process_t));
    if (!p)
        return NULL;

    memset(p, 0, sizeof(process_t));
    p->pid = next_pid++;
    return p;
}

static void setup_kernel_stack(process_t *p)
{
    p->kernel_stack = kmalloc(PROCESS_KERNEL_STACK);
    p->kernel_stack_top = (uint8_t *)p->kernel_stack + PROCESS_KERNEL_STACK;
}

static void setup_user_stack(process_t *p)
{
    p->user_stack = kmalloc(PROCESS_USER_STACK);
}

/* -----------------------------
 * CREATE KERNEL PROCESS
 * ----------------------------- */

process_t *process_create_kernel(const char *name,
                                void (*entry)(void))
{
    process_t *p = alloc_process();
    if (!p) return NULL;

    p->type = PROCESS_KERNEL;
    p->state = PROCESS_READY;
    p->entry = entry;

    setup_kernel_stack(p);

    snprintf(p->name, PROCESS_NAME_MAX, "%s", name);

    /* initial context */
    memset(&p->context, 0, sizeof(cpu_context_t));

    p->context.rsp = (uint64_t)p->kernel_stack_top;
    p->context.rip = (uint64_t)entry;
    p->context.rflags = 0x202;

    scheduler_add(p);
    return p;
}

/* -----------------------------
 * CREATE USER PROCESS
 * ----------------------------- */

process_t *process_create_user(const char *path,
                              int argc,
                              const char *argv[])
{
    process_t *p = alloc_process();
    if (!p) return NULL;

    p->type = PROCESS_USER;
    p->state = PROCESS_READY;

    setup_kernel_stack(p);
    setup_user_stack(p);

    snprintf(p->name, PROCESS_NAME_MAX, "%s", path);

    elf_image_info_t info = {0};
    void *entry = NULL;
    uint64_t user_stack = 0;

    userland_exec_prepare(path, argc, argv, &info, &entry, &user_stack);

    p->entry = entry;

    /* IMPORTANT: init CPU context */
    memset(&p->context, 0, sizeof(cpu_context_t));

    p->context.rip = (uint64_t)entry;
    p->context.rsp = user_stack;
    p->context.rflags = 0x202;

    scheduler_add(p);

    return p;
}

/* -----------------------------
 * EXIT
 * ----------------------------- */

void process_exit(int code)
{
    if (!current_process)
        return;

    current_process->exit_code = code;
    current_process->state = PROCESS_ZOMBIE;

    scheduler_yield();
}

/* -----------------------------
 * FIND
 * ----------------------------- */

process_t *process_find(uint32_t pid)
{
    process_t *p = process_list;

    while (p)
    {
        if (p->pid == pid)
            return p;

        p = p->next;
    }

    return NULL;
}

/* -----------------------------
 * SLEEP
 * ----------------------------- */

void process_sleep(uint64_t ticks)
{
    if (!current_process)
        return;

    current_process->state = PROCESS_SLEEPING;
    current_process->wakeup_tick = pit_ticks + ticks;

    scheduler_yield();
}

/* -----------------------------
 * INIT
 * ----------------------------- */

void process_init(void)
{
    process_list = NULL;
    next_pid = 1;
    current_process = NULL;
}

/* -----------------------------
 * FOR SCHEDULER USE ONLY
 * ----------------------------- */

void process_enqueue(process_t *p)
{
    p->next = process_list;
    process_list = p;
}

/* simple dequeue (not heavily used in RR) */
process_t *process_dequeue(void)
{
    if (!process_list)
        return NULL;

    process_t *p = process_list;
    process_list = p->next;
    return p;
}

process_t *process_fork(process_t *parent)
{
    process_t *child = alloc_process();
    if (!child)
        return NULL;

    uint32_t child_pid = child->pid;
    memcpy(child, parent, sizeof(process_t));

    child->pid = child_pid;
    child->state = PROCESS_READY;

    /* copy kernel stack */
    child->kernel_stack = kmalloc(PROCESS_KERNEL_STACK);
    memcpy(child->kernel_stack,
           parent->kernel_stack,
           PROCESS_KERNEL_STACK);

    child->kernel_stack_top =
        (uint8_t *)child->kernel_stack + PROCESS_KERNEL_STACK;

    /* copy user stack (simple clone, not COW yet) */
    child->user_stack = kmalloc(PROCESS_USER_STACK);
    memcpy(child->user_stack,
           parent->user_stack,
           PROCESS_USER_STACK);

    /* FIX context so child returns 0, parent gets child PID */
    child->context.rax = 0;          // fork() return in child
    parent->context.rax = child->pid; // fork() return in parent

    scheduler_add(child);

    return child;
}

void process_destroy(process_t *proc)
{
    if (!proc)
        return;
    proc->state = PROCESS_DEAD;
    if (proc->kernel_stack)
        kfree(proc->kernel_stack);
    if (proc->user_stack)
        kfree(proc->user_stack);
    kfree(proc);
}