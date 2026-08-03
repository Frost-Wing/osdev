#ifndef MULTITASKING_H
#define MULTITASKING_H

#include <basics.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TASK_TYPE_KERNEL = 0,
    TASK_TYPE_USERLAND = 1
} task_type_t;

typedef enum {
    TASK_STATE_READY = 0,
    TASK_STATE_RUNNING = 1,
    TASK_STATE_SLEEPING = 2,
    TASK_STATE_EXITED = 3
} task_state_t;

typedef struct task_info {
    uint32_t pid;
    task_type_t type;
    task_state_t state;
    int exit_code;
    uint64_t runtime_ticks;
    uint64_t wakeup_tick;
    uint32_t parent_pid;
    const char *name;
    const char *exe_path;
} task_info_t;

typedef struct user_task_spec {
    const char *path;
    int argc;
    const char *argv[32];
    uint32_t parent_pid;
    bool fork_child;
} user_task_spec_t;

typedef bool (*task_iter_cb_t)(const task_info_t *info, void *ctx);
typedef bool (*kernel_task_fn_t)(uint32_t pid, uint64_t now_ticks, void *ctx, int *exit_code);

typedef struct {
    uint64_t rip;
    uint64_t rsp;
    int started;
} user_runtime_t;

typedef struct task {
    uint32_t pid;
    task_type_t type;
    task_state_t state;
    int exit_code;
    uint64_t created_at_tick;
    uint64_t runtime_ticks;
    uint64_t wakeup_tick;
    uint32_t parent_pid;
    bool fork_child;
    char name[64];

    kernel_task_fn_t kernel_fn;
    void *kernel_ctx;

    user_task_spec_t user_spec;
    user_runtime_t user_runtime;
    struct task *next;

} task_t;

void multitasking_init(void);
void multitasking_on_pit_tick(uint64_t now_ticks);
void multitasking_pump(void);
task_t *multitasking_find_task(uint32_t pid);

uint32_t multitasking_spawn_kernel(const char *name, kernel_task_fn_t fn, void *ctx);
uint32_t multitasking_spawn_userland(const char *name, const user_task_spec_t *spec);

/*
 * Tiny process API for the rest of the kernel:
 *  - spawn_* creates runnable work and returns a Linux-style pid, or 0 on error.
 *  - exit/kill move a task to EXITED so the parent can wait4()/reap it.
 *  - sleep parks a task until the PIT tick reaches wakeup_tick.
 *  - yield gives another ready user task a chance to run cooperatively.
 */
bool multitasking_exit_task(uint32_t pid, int exit_code);
bool multitasking_kill_task(uint32_t pid, int signal);
bool multitasking_sleep_task(uint32_t pid, uint64_t wakeup_tick);
void multitasking_yield(void);
bool multitasking_update_user_image(uint32_t pid, const char *path, int argc, const char *const argv[]);
bool multitasking_reap_task(uint32_t pid, task_info_t *out_info);
bool multitasking_find_child(uint32_t parent_pid, int64_t pid_filter, bool exited_only, task_info_t *out_info);
bool multitasking_current_is_fork_child(void);
bool multitasking_get_task(uint32_t pid, task_info_t *out_info);
uint32_t multitasking_current_pid(void);

uint32_t multitasking_count_tasks(void);
uint32_t multitasking_count_running(void);

bool multitasking_for_each_task(task_iter_cb_t cb, void *ctx);

void multitasking_start_cursor_blink_task(void);
task_t *multitasking_get_current_task(void);

#endif
