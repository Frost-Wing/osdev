/**
 * @file scheduler.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2026-07-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#include <scheduler/scheduler.h>

#include <memory.h>
#include <pit.h>

static process_t *ready_head = NULL;
static process_t *ready_tail = NULL;

process_t *current_process = NULL;

void scheduler_add(process_t *proc)
{
    proc->next = NULL;

    if (!ready_tail)
    {
        ready_head = proc;
        ready_tail = proc;
        return;
    }

    ready_tail->next = proc;
    ready_tail = proc;
}

process_t *scheduler_next(void)
{
    process_t *p = ready_head;

    while (p)
    {
        if (p->state == PROCESS_READY)
            return p;

        p = p->next;
    }

    return NULL;
}

void scheduler_yield(void)
{
    if (!current_process)
        return;

    if (current_process->state == PROCESS_RUNNING)
        current_process->state = PROCESS_READY;

    if (ready_head == current_process)
    {
        ready_head = current_process->next;

        if (!ready_head)
            ready_tail = NULL;

        current_process->next = NULL;
        scheduler_add(current_process);
    }
}

void scheduler_sleep(process_t *proc, uint64_t ticks)
{
    proc->state = PROCESS_SLEEPING;
    proc->wakeup_tick = pit_ticks + ticks;
}

void scheduler_wakeup(uint64_t now)
{
    process_t *p = ready_head;

    while (p)
    {
        if (p->state == PROCESS_SLEEPING &&
            p->wakeup_tick <= now)
        {
            p->state = PROCESS_READY;
        }

        p = p->next;
    }
}

extern void context_switch(cpu_context_t *old,
                           cpu_context_t *new);

void scheduler_switch(process_t *next)
{
    if (!next)
        return;

    process_t *old = current_process;
    current_process = next;

    next->state = PROCESS_RUNNING;

    if (!old)
    {
        asm volatile(
            "mov %0, %%rsp\n"
            "jmp *%1\n"
            :
            : "r"(next->context.rsp),
              "r"(next->context.rip)
            : "memory");
    }

    context_switch(&old->context, &next->context);
}
void scheduler_init(void)
{
    ready_head = NULL;
    ready_tail = NULL;
    current_process = NULL;
}

void scheduler_remove(process_t *proc)
{
    if (!proc)
        return;

    process_t *prev = NULL;
    process_t *cur = ready_head;
    while (cur) {
        if (cur == proc)
            break;
        prev = cur;
        cur = cur->next;
    }
    if (!cur)
        return;
    if (prev)
        prev->next = cur->next;
    else
        ready_head = cur->next;
    if (ready_tail == cur)
        ready_tail = prev;
    cur->next = NULL;
}

process_t *scheduler_current(void)
{
    return current_process;
}

void scheduler_tick(trap_frame_t *frame)
{
    if (current_process && frame) {
        current_process->trapframe = frame;
        current_process->context.rax = frame->rax;
        current_process->context.rip = frame->rip;
        current_process->context.rsp = frame->rsp;
        current_process->context.rflags = frame->rflags;
    }

    process_t *next = scheduler_next();
    if (!next || next == current_process)
        return;

    scheduler_switch(next);
}
