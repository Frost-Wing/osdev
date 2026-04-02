#include <commands/commands.h>
#include <multitasking.h>

static const char* state_name(task_state_t state) {
    switch (state) {
        case TASK_STATE_READY: return "READY";
        case TASK_STATE_RUNNING: return "RUNNING";
        case TASK_STATE_EXITED: return "EXITED";
        default: return "UNKNOWN";
    }
}

static const char* type_name(task_type_t type) {
    switch (type) {
        case TASK_TYPE_KERNEL: return "KERNEL";
        case TASK_TYPE_USERLAND: return "USER";
        default: return "?";
    }
}

typedef struct {
    uint32_t rows;
} list_ctx_t;

static bool print_task_row(const task_info_t* info, void* ctx_ptr) {
    list_ctx_t* ctx = (list_ctx_t*)ctx_ptr;
    printf("%4u  %-6s %-8s exit=%d runtime_ticks=%u  %s",
           info->pid,
           type_name(info->type),
           state_name(info->state),
           info->exit_code,
           (uint32_t)info->runtime_ticks,
           info->name ? info->name : "(unnamed)");
    ctx->rows++;
    return true;
}

int cmd_tasks(int argc, char** argv) {
    (void)argc;
    (void)argv;

    list_ctx_t ctx = {0};

    printf("PID   TYPE   STATE    DETAILS");
    multitasking_for_each_task(print_task_row, &ctx);

    printf("total=%u active=%u",
           multitasking_count_tasks(),
           multitasking_count_running());

    return 0;
}
