/**
 * @file process.h
 * @author Pradosh
 * @brief FrostWing process management.
 *
 * Every process owns:
 *  - its own kernel stack
 *  - its own saved CPU context
 *  - its own address space (CR3)
 *
 * The scheduler switches between process_t objects.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>

#define PROCESS_NAME_MAX        64
#define PROCESS_KERNEL_STACK    0x4000
#define PROCESS_USER_STACK      0x4000

#define PROCESS_MAX_ARGUMENTS   32

typedef enum
{
    PROCESS_READY = 0,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_SLEEPING,
    PROCESS_ZOMBIE,
    PROCESS_DEAD
} process_state_t;

typedef enum
{
    PROCESS_KERNEL = 0,
    PROCESS_USER
} process_type_t;


/*
 * Saved CPU state.
 *
 * switch.S ONLY touches this structure.
 *
 * When a process is not running, these contain the
 * exact register values needed to resume it.
 */
typedef struct cpu_context
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;

    uint64_t rbx;
    uint64_t rbp;

    uint64_t rax;

    uint64_t rsp;
    uint64_t rip;

    uint64_t rflags;
} cpu_context_t;


/*
 * Trap frame pushed by interrupts/syscalls.
 *
 * entry.S builds this.
 *
 * scheduler receives a pointer to this.
 */
typedef struct trap_frame
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;

    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;

    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t vector;
    uint64_t error;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;

} trap_frame_t;


struct process;

typedef struct process process_t;


/*
 * Main process descriptor.
 */
struct process
{
    uint32_t pid;

    process_type_t type;

    process_state_t state;

    cpu_context_t context;

    trap_frame_t *trapframe;

    /*
     * Physical address of PML4.
     */
    uint64_t cr3;

    /*
     * Kernel stack.
     */
    void *kernel_stack;

    void *kernel_stack_top;

    /*
     * User stack.
     */
    void *user_stack;

    /*
     * Entry point.
     */
    void *entry;

    int exit_code;

    uint64_t wakeup_tick;

    process_t *parent;

    process_t *next;

    char name[PROCESS_NAME_MAX];

    int argc;
    const char *argv[PROCESS_MAX_ARGUMENTS];
};


/*
 * Current process.
 */
extern process_t *current_process;


/* Creation */

process_t *process_create_kernel(
    const char *name,
    void (*entry)(void));

process_t *process_create_user(
    const char *path,
    int argc,
    const char *argv[]);


/* Destroy */
void process_destroy(process_t *proc);


/* Exit */
void process_exit(int code);


/* Lookup */
process_t *process_find(uint32_t pid);


/* Sleep */
void process_sleep(uint64_t ticks);


/* Ready queue */
void process_enqueue(process_t *proc);

process_t *process_dequeue(void);


/* Init */
void process_init(void);

#endif