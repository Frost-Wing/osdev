/**
 * @file multitasking.h
 * @brief Task creation, scheduling, and process-management interfaces.
 */
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

typedef struct user_task_spec {
    const char *path;
    int argc;
    const char *argv[32];
    uint32_t parent_pid;
    bool fork_child;
    /* TTY_INDEX_CURRENT inherits the spawning task's terminal. */
    uint8_t tty_index;
} user_task_spec_t;

typedef bool (*kernel_task_fn_t)(uint32_t pid, uint64_t now_ticks, void *ctx, int *exit_code);

typedef struct task_info {
    uint32_t pid;
    task_type_t type;
    task_state_t state;
    int exit_code;
    uint64_t created_at_tick;
    uint64_t runtime_ticks;
    uint64_t wakeup_tick;
    uint32_t parent_pid;
    uint8_t tty_index;
    char name[64];
    char exe_path[128];
} task_info_t;

/**
 * @brief Spawn a kernel task and transfer ownership of its context.
 *
 * The task subsystem frees @p ctx when the task is cleaned up.
 */
uint32_t multitasking_spawn_kernel_owned(const char *name, kernel_task_fn_t fn, void *ctx);

typedef bool (*task_iter_cb_t)(const task_info_t *info, void *ctx);

typedef struct {
    uint64_t rip;
    uint64_t rsp;
    int started;
} user_runtime_t;

typedef struct {
    uint64_t handler;
    uint64_t flags;
    uint64_t restorer;
    uint64_t mask;
} task_signal_action_t;

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
    uint8_t tty_index;
    char name[64];

    kernel_task_fn_t kernel_fn;
    void *kernel_ctx;

    bool owns_kernel_ctx;

    user_task_spec_t user_spec;
    user_runtime_t user_runtime;
    task_signal_action_t signal_actions[65];
    uint64_t rseq_area;
    uint32_t rseq_signature;
    struct task *next;

} task_t;

/** @brief Return the current scheduler tick count. */
uint64_t multitasking_now_ticks(void);

/** @brief Return the highest task identifier allocated so far. */
uint32_t multitasking_last_pid(void);

/**
 * @brief Copy a task's arguments in Linux command-line format.
 *
 * @param pid Task identifier.
 * @param buf Destination buffer.
 * @param size Buffer capacity in bytes.
 * @return Number of bytes copied, or -1 if the task does not exist.
 */
int multitasking_get_cmdline(uint32_t pid, char *buf, size_t size);

/** @brief Initialize the task scheduler. */
void multitasking_init(void);

/** @brief Process a timer tick and update task scheduling state. */
void multitasking_on_pit_tick(uint64_t now_ticks);

/** @brief Run pending cooperative task work. */
void multitasking_pump(void);

/** @brief Find a task by its identifier. */
task_t *multitasking_find_task(uint32_t pid);

/** @brief Set the task treated as current by the scheduler. */
void multitasking_set_current_pid(uint32_t pid);

/** @brief Spawn a kernel task and return its identifier. */
uint32_t multitasking_spawn_kernel(const char *name, kernel_task_fn_t fn, void *ctx);

/** @brief Spawn a userland task and return its identifier. */
uint32_t multitasking_spawn_userland(const char *name, const user_task_spec_t *spec);

/**
 * @brief Mark a task as exited with the supplied status.
 *
 * Exited tasks remain available for their parent to reap.
 */
bool multitasking_exit_task(uint32_t pid, int exit_code);

/** @brief Send a signal to a task and mark it for termination when applicable. */
bool multitasking_kill_task(uint32_t pid, int signal);

/** @brief Put a task to sleep until the specified scheduler tick. */
bool multitasking_sleep_task(uint32_t pid, uint64_t wakeup_tick);

/** @brief Yield the current task's execution opportunity. */
void multitasking_yield(void);

/** @brief Replace the user image and arguments of an existing task. */
bool multitasking_update_user_image(uint32_t pid, const char *path, int argc, const char *const argv[]);

/** @brief Reap an exited task and return its final status. */
bool multitasking_reap_task(uint32_t pid, task_info_t *out_info);

/** @brief Find a child task matching the requested process filter. */
bool multitasking_find_child(uint32_t parent_pid, int64_t pid_filter, bool exited_only, task_info_t *out_info);

/** @brief Check whether the current task was created as a fork child. */
bool multitasking_current_is_fork_child(void);

/** @brief Retrieve task information by identifier. */
bool multitasking_get_task(uint32_t pid, task_info_t *out_info);

/** @brief Return the current task identifier. */
uint32_t multitasking_current_pid(void);
/**
 * @brief Return the current task's terminal, or the visible terminal if none is active.
 */
uint8_t multitasking_current_tty(void);

/** @brief Return the number of tasks, including exited tasks not yet reaped. */
uint32_t multitasking_count_tasks(void);

/** @brief Return the number of tasks that are ready or running. */
uint32_t multitasking_count_running(void);

/** @brief Visit task records until the callback requests that iteration stop. */
bool multitasking_for_each_task(task_iter_cb_t cb, void *ctx);

/** @brief Return the current task record, or NULL when no task is active. */
task_t *multitasking_get_current_task(void);

/** @brief Run the multitasking subsystem's internal self-tests. */
void multitasking_selftest(void);

#endif
