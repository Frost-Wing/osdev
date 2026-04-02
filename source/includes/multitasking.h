#ifndef MULTITASKING_H
#define MULTITASKING_H

#include <basics.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TASK_TYPE_KERNEL = 0,
    TASK_TYPE_USERLAND = 1
} task_type_t;

typedef enum {
    TASK_STATE_READY = 0,
    TASK_STATE_RUNNING = 1,
    TASK_STATE_EXITED = 2
} task_state_t;

typedef struct task_info {
    uint32_t pid;
    task_type_t type;
    task_state_t state;
    int exit_code;
    uint64_t runtime_ticks;
    const char* name;
} task_info_t;

typedef struct user_task_spec {
    const char* path;
    int argc;
    const char* argv[32];
} user_task_spec_t;

typedef bool (*task_iter_cb_t)(const task_info_t* info, void* ctx);
typedef bool (*kernel_task_fn_t)(uint32_t pid, uint64_t now_ticks, void* ctx, int* exit_code);

void multitasking_init(void);
void multitasking_on_pit_tick(uint64_t now_ticks);
void multitasking_pump(void);

uint32_t multitasking_spawn_kernel(const char* name, kernel_task_fn_t fn, void* ctx);
uint32_t multitasking_spawn_userland(const char* name, const user_task_spec_t* spec);

bool multitasking_exit_task(uint32_t pid, int exit_code);
bool multitasking_get_task(uint32_t pid, task_info_t* out_info);
uint32_t multitasking_current_pid(void);

uint32_t multitasking_count_tasks(void);
uint32_t multitasking_count_running(void);

bool multitasking_for_each_task(task_iter_cb_t cb, void* ctx);

void multitasking_start_cursor_blink_task(void);

#endif
