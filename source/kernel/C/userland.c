#include <stdint.h>
#include <memory.h>
#include <basics.h>
#include <paging.h>
#include <userland.h>
#include <executables/elf.h>
#include <filesystems/vfs.h>
#include <heap.h>
#include <tss.h>
#include <tty.h>
#include <debugger.h>
#include <cc-asm.h>

// Defined in syscalls.c. Keeps sys_fork()'s notion of "what's currently
// running" accurate - see the call site in userland_exec() below.
extern void record_exec_context(const char* target, const char* const* argv);


static uint64_t user_heap_break = USER_HEAP_VADDR;
static uint64_t user_heap_mapped_end = USER_HEAP_VADDR;

static uint64_t user_mmap_cursor = USER_MMAP_VADDR;
static uint64_t user_mmap_end = USER_MMAP_VADDR;
static volatile bool userland_running = false;
static uint64_t userland_saved_kernel_stack_top = 0;
static uint64_t userland_saved_tss_rsp0 = 0;
volatile bool userland_should_return_kernel = false;
volatile uint64_t userland_resume_rip = 0;
volatile uint64_t userland_resume_rsp = 0;
volatile uint64_t userland_resume_rbx = 0;
volatile uint64_t userland_resume_rbp = 0;
volatile uint64_t userland_resume_r12 = 0;
volatile uint64_t userland_resume_r13 = 0;
volatile uint64_t userland_resume_r14 = 0;
volatile uint64_t userland_resume_r15 = 0;
volatile uint64_t userland_resume_ret_rip = 0;
volatile uint64_t userland_resume_ret_rsp = 0;
static volatile int userland_last_exit_code = 0;

/*
 * Reentrancy support.
 *
 * userland_exec() can legitimately be called while an *outer* call to
 * userland_exec() is still alive further down the C call stack - e.g.
 * sh (outer) blocks in sys_wait4() -> multitasking_pump(), which calls
 * userland_exec() again (inner) to actually run a forked child. The
 * globals above always describe the *currently active* (innermost)
 * frame - other code (exception handlers, userland_prepare_exit(),
 * etc.) keeps working unchanged. What's new is:
 *
 *   1. Every nesting depth gets its OWN 16KB kernel/syscall stack
 *      buffer, so an inner frame's syscalls can never grow down through
 *      memory an outer, still-suspended frame's C stack still occupies.
 *   2. On entry, the previous (outer) frame's globals are pushed onto
 *      userland_frame_stack[] before being overwritten with the new
 *      frame's values. On exit, they are popped back so the outer frame
 *      resumes with its own resume/exception state intact instead of
 *      zeroed out from under it.
 */
#define USERLAND_MAX_DEPTH 8

typedef struct {
    uint64_t saved_kernel_stack_top;
    uint64_t saved_tss_rsp0;
    uint64_t resume_rip;
    uint64_t resume_rsp;
    uint64_t resume_rbx;
    uint64_t resume_rbp;
    uint64_t resume_r12;
    uint64_t resume_r13;
    uint64_t resume_r14;
    uint64_t resume_r15;
    uint64_t resume_ret_rip;
    uint64_t resume_ret_rsp;
    int      last_exit_code;
} userland_saved_frame_t;

static userland_saved_frame_t userland_frame_stack[USERLAND_MAX_DEPTH];
static int userland_depth = 0; /* number of active (nested) userland_exec frames */

__attribute__((aligned(16)))
static uint8_t userland_syscall_stacks[USERLAND_MAX_DEPTH][0x4000];

/* Save whatever is currently in the "active frame" globals (the outer
 * frame we're about to supersede) and reserve a new depth slot. Returns
 * the index of the new frame's dedicated stack buffer, or -1 if we've
 * nested deeper than we ever expect to (a real bug elsewhere, not a
 * resource we should silently corrupt through). */
static int userland_push_frame(void) {
    if (userland_depth >= USERLAND_MAX_DEPTH)
        return -1;

    userland_saved_frame_t* f = &userland_frame_stack[userland_depth];
    f->saved_kernel_stack_top = userland_saved_kernel_stack_top;
    f->saved_tss_rsp0         = userland_saved_tss_rsp0;
    f->resume_rip             = userland_resume_rip;
    f->resume_rsp             = userland_resume_rsp;
    f->resume_rbx             = userland_resume_rbx;
    f->resume_rbp             = userland_resume_rbp;
    f->resume_r12             = userland_resume_r12;
    f->resume_r13             = userland_resume_r13;
    f->resume_r14             = userland_resume_r14;
    f->resume_r15             = userland_resume_r15;
    f->resume_ret_rip         = userland_resume_ret_rip;
    f->resume_ret_rsp         = userland_resume_ret_rsp;
    f->last_exit_code         = userland_last_exit_code;

    return userland_depth++;
}

/* Undo userland_push_frame(): pop the outer frame's state back into the
 * active globals. Returns true if an outer frame is still active (so the
 * caller should keep userland_running = true), false if this was the
 * outermost frame (fully unwound, nothing left to resume). */
static bool userland_pop_frame(void) {
    if (userland_depth == 0)
        return false;

    userland_depth--;

    if (userland_depth == 0) {
        userland_resume_rip = 0;
        userland_resume_rsp = 0;
        userland_resume_rbx = 0;
        userland_resume_rbp = 0;
        userland_resume_r12 = 0;
        userland_resume_r13 = 0;
        userland_resume_r14 = 0;
        userland_resume_r15 = 0;
        userland_resume_ret_rip = 0;
        userland_resume_ret_rsp = 0;
        userland_saved_kernel_stack_top = 0;
        userland_saved_tss_rsp0 = 0;
        userland_last_exit_code = 0;
        return false;
    }

    userland_saved_frame_t* f = &userland_frame_stack[userland_depth - 1];
    userland_saved_kernel_stack_top = f->saved_kernel_stack_top;
    userland_saved_tss_rsp0         = f->saved_tss_rsp0;
    userland_resume_rip             = f->resume_rip;
    userland_resume_rsp             = f->resume_rsp;
    userland_resume_rbx             = f->resume_rbx;
    userland_resume_rbp             = f->resume_rbp;
    userland_resume_r12             = f->resume_r12;
    userland_resume_r13             = f->resume_r13;
    userland_resume_r14             = f->resume_r14;
    userland_resume_r15             = f->resume_r15;
    userland_resume_ret_rip         = f->resume_ret_rip;
    userland_resume_ret_rsp         = f->resume_ret_rsp;
    userland_last_exit_code         = f->last_exit_code;
    return true;
}
static inline void wrmsr64_local(uint32_t msr, uint64_t value);
static void userland_unmap_all(void);
void userland_heap_init(void);

__attribute__((noinline, noreturn)) static void userland_finish_exit(void) {
    uint64_t return_rip = userland_resume_ret_rip;
    uint64_t return_rsp = userland_resume_ret_rsp;
    int exit_code = userland_last_exit_code;

    userland_should_return_kernel = false;

    /* Restore the kernel stack pointer this frame overwrote back to
     * whatever it was before this frame started - the outer frame's own
     * dedicated buffer (or the true original kernel stack, if this was
     * the outermost frame). This must happen using *this* frame's saved
     * values, before we pop and lose them. */
    kernel_stack_top = userland_saved_kernel_stack_top;
    tss.rsp0 = userland_saved_tss_rsp0;

    /* Pop back to the outer frame's resume/exception state (if any). If
     * there's still an outer frame active, a process is still logically
     * "running" (blocked mid-syscall) even though nothing is in ring3
     * right now, so keep userland_running true for it. Only the truly
     * outermost exit clears it. */
    userland_running = userland_pop_frame();

    wrmsr64_local(IA32_FS_BASE_MSR, 0);
    userland_unmap_all();
    userland_heap_init();
    tty_flush_input();
    printf(blue_color "\n[process exited with code %d]" reset_color, exit_code);
    asm volatile("sti");
    
    asm volatile(
        "xor %%rax, %%rax\n"
        "mov %0, %%rsp\n"
        "jmp *%1\n"
        :
        : "r"(return_rsp), "r"(return_rip)
        : "memory", "rax");
    __builtin_unreachable();
}

static void debug_dump_initial_stack(uint64_t stack_top) {
    uint64_t* words = (uint64_t*)stack_top;
    debug_printf("userland: initial rsp=%x argc=%u argv0=%x argv1=%x env0=%x aux0=%x aux1=%x\n",
                 stack_top,
                 (uint32_t)words[0],
                 words[1],
                 words[2],
                 words[(uint32_t)words[0] + 2],
                 words[(uint32_t)words[0] + 4],
                 words[(uint32_t)words[0] + 5]);
}

static uint64_t rdtsc64_local(void) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr64_local(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static uint64_t align_up_u64(uint64_t value, uint64_t align) {
    if (align <= 1)
        return value;
    return (value + align - 1) & ~(align - 1);
}

__attribute__((unused)) static uint64_t max_u64(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}

static uint64_t push_bytes_to_stack(uint64_t* stack_ptr, const void* src, uint64_t len) {
    *stack_ptr -= len;
    memcpy((void*)*stack_ptr, src, len);
    return *stack_ptr;
}

static uint64_t push_cstr_to_stack(uint64_t* stack_ptr, const char* str) {
    return push_bytes_to_stack(stack_ptr, str, (uint64_t)strlen(str) + 1U);
}

static int string_array_count(const char* const* arr) {
    if (!arr)
        return 0;

    int count = 0;
    while (arr[count] != NULL)
        count++;

    return count;
}

static const char* const default_envp[] = {
    "HOME=/",
    "PATH=/",
    "TERM=linux",
    "USER=none",
    "SHLVL=1",
    NULL
};

static uint64_t build_initial_user_stack(const char* exec_path,
                                         int argc,
                                         const char* const* argv,
                                         const char* const* envp,
                                         const elf_image_info_t* image_info)
{
    const char* const* final_envp = envp ? envp : default_envp;
    int envc = string_array_count(final_envp);
    int argvc = argc;
    uint64_t stack_ptr = USER_STACK_TOP;
    uint64_t random_addr = 0;
    uint64_t execfn_addr = 0;
    uint64_t platform_addr = 0;
    uint64_t argv_addrs[32];
    uint64_t env_addrs[32];
    uint8_t random_bytes[16];
    char exec_name_buf[256];
    auxv_pair_t auxv[USER_AUXV_MAX];
    int auxc = 0;

    if (argvc < 0)
        argvc = 0;
    if (argvc > 32)
        argvc = 32;
    if (envc > 32)
        envc = 32;

    // --- blob area (strings + random) ---
    for (int i = 0; i < (int)sizeof(random_bytes); ++i)
        random_bytes[i] = (uint8_t)(rdtsc64_local() >> ((i & 7) * 8));
    random_addr = push_bytes_to_stack(&stack_ptr, random_bytes, sizeof(random_bytes));

    platform_addr = push_cstr_to_stack(&stack_ptr, "x86_64");

    snprintf(exec_name_buf, sizeof(exec_name_buf), "%s", exec_path ? exec_path : "");
    execfn_addr = push_cstr_to_stack(&stack_ptr, exec_name_buf);

    for (int i = envc - 1; i >= 0; --i)
        env_addrs[i] = push_cstr_to_stack(&stack_ptr, final_envp[i] ? final_envp[i] : "");

    for (int i = argvc - 1; i >= 0; --i) {
        const char* s = (argv && argv[i]) ? argv[i] : "";
        argv_addrs[i] = push_cstr_to_stack(&stack_ptr, s);
    }

    // --- auxiliary vector ---
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_PHDR, image_info ? image_info->phdr_addr : 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_PHENT, image_info ? image_info->phentsize : 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_PHNUM, image_info ? image_info->phnum : 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_PAGESZ, PAGE_SIZE };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_BASE, 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_FLAGS, 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_ENTRY, image_info ? image_info->entry : 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_UID, 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_EUID, 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_GID, 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_EGID, 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_HWCAP, 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_CLKTCK, 100 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_SECURE, 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_RANDOM, random_addr };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_HWCAP2, 0 };
    auxv[auxc++] = (auxv_pair_t){ LINUX_AT_PLATFORM, platform_addr };
    if (execfn_addr && auxc < USER_AUXV_MAX)
        auxv[auxc++] = (auxv_pair_t){ LINUX_AT_EXECFN, execfn_addr };

    // --- pointer frame: argc, argv[], NULL, envp[], NULL, auxv[], AT_NULL ---
    uint64_t frame_words =
        1 +
        (uint64_t)argvc + 1 +
        (uint64_t)envc + 1 +
        ((uint64_t)(auxc + 1) * 2);
    uint64_t frame_size = frame_words * sizeof(uint64_t);
    uint64_t frame_ptr = stack_ptr - frame_size;

    // Linux/x86_64 entry ABI expects %rsp % 16 == 8.
    frame_ptr &= ~0x7ULL;
    if ((frame_ptr & 0xFULL) != 8)
        frame_ptr -= 8;

    uint64_t* out = (uint64_t*)frame_ptr;
    *out++ = (uint64_t)argvc;

    for (int i = 0; i < argvc; ++i)
        *out++ = argv_addrs[i];
    *out++ = 0;

    for (int i = 0; i < envc; ++i)
        *out++ = env_addrs[i];
    *out++ = 0;

    for (int i = 0; i < auxc; ++i) {
        *out++ = auxv[i].key;
        *out++ = auxv[i].value;
    }

    *out++ = LINUX_AT_NULL;
    *out++ = 0;
    return frame_ptr;
}

static int init_user_tls(const elf_image_info_t* image_info) {
    uint64_t tls_memsz = image_info ? image_info->tls_memsz : 0;
    uint64_t tls_filesz = image_info ? image_info->tls_filesz : 0;
    uint64_t tls_align = (image_info && image_info->tls_align) ? image_info->tls_align : 1;

    // ELF TLS alignment should be a power-of-two; be defensive.
    if ((tls_align & (tls_align - 1)) != 0)
        tls_align = 1;

    // IMPORTANT:
    // - static TLS block uses ELF alignment (do NOT force 16 here)
    // - TCB remains 16-byte aligned
    uint64_t tls_block_size = tls_memsz ? align_up_u64(tls_memsz, tls_align) : 0;
    uint64_t tcb_addr = align_up_u64(USER_TLS_VADDR + tls_block_size, 16);
    uint64_t tls_block_addr = tls_block_size ? (tcb_addr - tls_block_size) : tcb_addr;
    uint64_t tls_end = align_up_u64(tcb_addr + sizeof(glibc_tls_block_t), PAGE_SIZE);

    uint64_t guard = rdtsc64_local() ^ 0x9e3779b97f4a7c15ULL;
    guard &= ~0xFFULL; // canonical stack canary convention: low byte = 0

    if (tls_end > USER_TLS_VADDR + USER_TLS_REGION_SIZE) {
        eprintf("userland: TLS region too small end=%x limit=%x",
                tls_end, USER_TLS_VADDR + USER_TLS_REGION_SIZE);
        return -1;
    }

    for (uint64_t vaddr = USER_TLS_VADDR; vaddr < tls_end; vaddr += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        map_user_page(vaddr, phys, USER_DATA_FLAGS);
    }

    memset((void*)USER_TLS_VADDR, 0, tls_end - USER_TLS_VADDR);

    if (tls_filesz && image_info && image_info->tls_template)
        memcpy((void*)tls_block_addr, image_info->tls_template, tls_filesz);
    if (tls_memsz > tls_filesz)
        memset((void*)(tls_block_addr + tls_filesz), 0, tls_memsz - tls_filesz);

    glibc_tls_block_t* tls = (glibc_tls_block_t*)tcb_addr;
    glibc_tcb_head_t* tcb = &tls->head;

    tls->dtv[0].counter = 1;
    tls->dtv[1].pointer.val = (void*)tls_block_addr;
    tls->dtv[1].pointer.to_free = NULL;

    tcb->tcb = tcb_addr;
    tcb->dtv = &tls->dtv[1];
    tcb->self = tcb_addr;
    tcb->multiple_threads = 0;
    tcb->gscope_flag = 0;
    tcb->sysinfo = 0;
    tcb->stack_guard = guard;
    tcb->pointer_guard = guard ^ 0xfeedfacecafebeefULL;
    tcb->feature_1 = 0;
    tcb->ssp_base = 0;

    debug_printf("userland: tls base=%x block=%x filesz=%u memsz=%u align=%u dtv=%x stack_guard=%x pointer_guard=%x\n",
                 tcb_addr, tls_block_addr, tls_filesz, tls_memsz, tls_align,
                 tcb->dtv, tcb->stack_guard, tcb->pointer_guard);

    wrmsr64_local(IA32_FS_BASE_MSR, tcb_addr);
    return 0;
}

static void map_user_stack(void) {
    uint64_t stack_top = USER_STACK_TOP;

    for (uint64_t off = 0; off < USER_STACK_SIZE; off += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        uint64_t vaddr = stack_top - off - PAGE_SIZE;
        map_user_page(vaddr, phys, USER_DATA_FLAGS);
    }
}

static void map_user_range(uint64_t start, uint64_t end, uint64_t flags) {
    uint64_t aligned_start = start & ~(PAGE_SIZE - 1);
    uint64_t aligned_end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint64_t vaddr = aligned_start; vaddr < aligned_end; vaddr += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        map_user_page(vaddr, phys, flags);
    }
}

static void unmap_user_range(uint64_t start, uint64_t end) {
    uint64_t aligned_start = start & ~(PAGE_SIZE - 1);
    uint64_t aligned_end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint64_t vaddr = aligned_start; vaddr < aligned_end; vaddr += PAGE_SIZE)
        unmap_user_page(vaddr);
}

static void userland_unmap_all(void) {
    unmap_user_range(USER_CODE_VADDR, USER_HEAP_VADDR);
    unmap_user_range(USER_HEAP_VADDR, USER_HEAP_VADDR + USER_HEAP_SIZE);
    unmap_user_range(USER_MMAP_VADDR, USER_MMAP_VADDR + USER_MMAP_SIZE);
    unmap_user_range(USER_TLS_VADDR, USER_TLS_VADDR + USER_TLS_REGION_SIZE);
    unmap_user_range(USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_TOP);
}

void userland_heap_init(void) {
    user_heap_break = USER_HEAP_VADDR;
    user_heap_mapped_end = USER_HEAP_VADDR;

    user_mmap_cursor = USER_MMAP_VADDR;
    user_mmap_end = USER_MMAP_VADDR + USER_MMAP_SIZE;
}

uint64_t userland_brk(uint64_t requested_break) {
    uint64_t user_heap_end = USER_HEAP_VADDR + USER_HEAP_SIZE;

    if (requested_break == 0) {
        return user_heap_break;
    }

    if (requested_break < USER_HEAP_VADDR || requested_break > user_heap_end) {
        return user_heap_break;
    }

    if (requested_break > user_heap_mapped_end) {
        map_user_range(user_heap_mapped_end, requested_break, USER_DATA_FLAGS);
        user_heap_mapped_end = (requested_break + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }

    user_heap_break = requested_break;
    return user_heap_break;
}

uint64_t userland_mmap_anon(uint64_t length) {
    if (length == 0) {
        return 0;
    }

    uint64_t aligned_len = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (user_mmap_cursor + aligned_len > user_mmap_end) {
        return 0;
    }

    uint64_t mapping_base = user_mmap_cursor;
    map_user_range(mapping_base, mapping_base + aligned_len, USER_DATA_FLAGS);
    user_mmap_cursor += aligned_len;

    return mapping_base;
}

bool userland_prepare_exit(syscall_frame_t* frame, uint64_t exit_code) {
    (void)frame;

    if (!userland_running || !userland_resume_rip || !userland_resume_rsp)
        return false;

    userland_last_exit_code = (int)exit_code;
    userland_should_return_kernel = true;
    return true;
}

bool userland_is_running(void) {
    return userland_running;
}

static int userland_exception_exit_code(uint64_t int_no) {
    switch (int_no) {
        case 0:  // divide by zero
            return 136;
        case 6:  // invalid opcode
            return 132;
        case 13: // general protection fault
        case 14: // page fault
            return 139;
        default:
            return 128 + (int)(int_no & 0x7F);
    }
}

void userland_abort_from_exception(uint64_t int_no, uint64_t err_code, uint64_t fault_rip) {
    if (!userland_running || !userland_resume_rip || !userland_resume_rsp)
        hcf2();

    userland_last_exit_code = userland_exception_exit_code(int_no);
    eprintf("[userland] fatal exception: int=%02u err=0x%02X rip=0x%X -> exit=%02d",
            int_no,
            err_code,
            fault_rip,
            userland_last_exit_code);

    asm volatile(
        "cli\n"
        "mov %0, %%rbx\n"
        "mov %1, %%rbp\n"
        "mov %2, %%r12\n"
        "mov %3, %%r13\n"
        "mov %4, %%r14\n"
        "mov %5, %%r15\n"
        "mov %6, %%rsp\n"
        "jmp *%7\n"
        :
        : "r"(userland_resume_rbx),
          "r"(userland_resume_rbp),
          "r"(userland_resume_r12),
          "r"(userland_resume_r13),
          "r"(userland_resume_r14),
          "r"(userland_resume_r15),
          "r"(userland_resume_rsp),
          "r"(userland_resume_rip)
        : "memory");

    __builtin_unreachable();
}


/**
 * @brief Enter userland (ring 3) at a specific userspace RIP.
 */
void enter_userland_at(uint64_t code_entry) {
    uint64_t stack_top = USER_STACK_TOP; // 8-bit alignment

    map_user_stack();
    userland_heap_init();
    if (init_user_tls(NULL) != 0)
        return;

    // printf("Switching to userland at 0x%x with stack 0x%x", code_entry, stack_top);

    asm volatile (
        "cli\n"
        "mov %0, %%r11\n"
        "mov %1, %%r10\n"
        "xor %%rax, %%rax\n"
        "xor %%rbx, %%rbx\n"
        "xor %%rcx, %%rcx\n"
        "xor %%rdx, %%rdx\n"
        "xor %%rsi, %%rsi\n"
        "xor %%rdi, %%rdi\n"
        "xor %%r8, %%r8\n"
        "xor %%r9, %%r9\n"
        "pushq $0x23\n"
        "pushq %%r11\n"
        "pushq $0x202\n"
        "pushq $0x1B\n"
        "pushq %%r10\n"
        "iretq\n"
        :
        : "r"(stack_top), "r"(code_entry)
        : "memory", "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"
    );

    return;
}


void userland_exec_prepare(
    const char *path,
    int argc,
    const char *argv[],
    elf_image_info_t *out_info,
    void **out_entry,
    uint64_t *out_stack)
{
    elf_image_info_t image_info = {0};
    void *entry = elf_load_from_vfs_ex(path, &image_info);
    if (!entry) {
        if (out_entry)
            *out_entry = NULL;
        if (out_stack)
            *out_stack = 0;
        return;
    }

    map_user_stack();
    userland_heap_init();

    if (out_info)
        *out_info = image_info;
    if (out_entry)
        *out_entry = entry;
    if (out_stack)
        *out_stack = build_initial_user_stack(path, argc, argv, NULL, &image_info);

    if (image_info.tls_template)
        kfree(image_info.tls_template);
}

int userland_exec(const char* path, int argc, const char* const* argv, const char* const* envp) {
    elf_image_info_t image_info = {0};
    void* entry = elf_load_from_vfs_ex(path, &image_info);
    if (!entry)
        return -1;

    // This is the one place every userland process passes through right
    // before it actually starts running - whether it got here as the
    // initial process spawned by kernel init, via sys_execve(), or as a
    // fork()-spawned child pumped by multitasking_pump(). Recording it
    // here (rather than only in sys_execve()) guarantees current_exec_path
    // always reflects the program that is truly executing right now, so a
    // later fork() from that process always has a valid target to
    // respawn instead of falling back to the "/" default and failing with
    // ENOSYS ("fork not implemented").
    record_exec_context(path, argv);

    map_user_stack();
    userland_heap_init();
    if (init_user_tls(&image_info) != 0) {
        if (image_info.tls_template)
            kfree(image_info.tls_template);
        return -1;
    }
    if (image_info.tls_template)
        kfree(image_info.tls_template);

    uint64_t stack_top = build_initial_user_stack(path, argc, argv, envp, &image_info);

    debug_printf("userland: exec path=%s entry=%x phdr=%x phentsz=%u phnum=%u tls_mem=%u tls_file=%u stack=%x\n",
                 path,
                 entry,
                 image_info.phdr_addr,
                 image_info.phentsize,
                 image_info.phnum,
                 image_info.tls_memsz,
                 image_info.tls_filesz,
                 stack_top);
    debug_dump_initial_stack(stack_top);

    /* Save the outer frame's resume/exception state (if any is currently
     * active) before we overwrite the globals below, and grab this
     * nesting depth's own dedicated stack buffer. This is what makes it
     * safe for this call to happen while an outer userland_exec() is
     * still alive further down the C stack (e.g. sh blocked in
     * sys_wait4() -> multitasking_pump() running a forked child). */
    int frame_depth = userland_push_frame();
    if (frame_depth < 0) {
        eprintf("[userland] exec nesting too deep (max %d), refusing", USERLAND_MAX_DEPTH);
        return -1;
    }

    uint64_t kernel_rsp = 0;
    asm volatile("mov %%rsp, %0" : "=r"(kernel_rsp));
    asm volatile("mov %%rbx, %0" : "=r"(userland_resume_rbx));
    asm volatile("mov %%rbp, %0" : "=r"(userland_resume_rbp));
    asm volatile("mov %%r12, %0" : "=r"(userland_resume_r12));
    asm volatile("mov %%r13, %0" : "=r"(userland_resume_r13));
    asm volatile("mov %%r14, %0" : "=r"(userland_resume_r14));
    asm volatile("mov %%r15, %0" : "=r"(userland_resume_r15));
    void* frame = __builtin_frame_address(0);
    userland_resume_ret_rip = (uint64_t)__builtin_return_address(0);
    userland_resume_ret_rsp = (uint64_t)frame + 16;
    userland_resume_rsp = kernel_rsp;
    userland_resume_rip = (uint64_t)userland_finish_exit;
    userland_should_return_kernel = false;
    userland_last_exit_code = 0;
    userland_running = true;
    userland_saved_kernel_stack_top = kernel_stack_top;
    userland_saved_tss_rsp0 = tss.rsp0;
    kernel_stack_top = (uint64_t)&userland_syscall_stacks[frame_depth][sizeof(userland_syscall_stacks[frame_depth])];
    tss.rsp0 = kernel_stack_top;

    asm volatile (
        "cli\n"
        "mov %0, %%r11\n"
        "mov %1, %%r10\n"
        "xor %%rax, %%rax\n"
        "xor %%rbx, %%rbx\n"
        "xor %%rcx, %%rcx\n"
        "xor %%rdx, %%rdx\n"
        "xor %%rsi, %%rsi\n"
        "xor %%rdi, %%rdi\n"
        "xor %%r8, %%r8\n"
        "xor %%r9, %%r9\n"
        "pushq $0x23\n"
        "pushq %%r11\n"
        "pushq $0x202\n"
        "pushq $0x1B\n"
        "pushq %%r10\n"
        "iretq\n"
        :
        : "r"(stack_top), "r"((uint64_t)entry)
        : "memory", "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"
    );

    __builtin_unreachable();
}
/**
 * @brief execve() semantics for a process that is *already running*
 * (called from sys_execve(), from inside that process's own syscall).
 *
 * Unlike userland_exec(), this must NOT push a new reentrancy frame: a
 * real execve() replaces the current program image in place but keeps
 * the same "who resumes when this process eventually exits" identity.
 * If it pushed a new frame like a fresh spawn does, the *next* exit
 * would unwind into the middle of this now-abandoned sys_execve() call
 * instead of back to whoever originally started this process (e.g.
 * multitasking_pump() or the kernel shell's "exec" command) - which
 * would resume a stale/nonexistent ring3 context and crash.
 *
 * So: reuse the current depth's kernel stack buffer and leave the
 * resume_ret_rip/resume_ret_rsp/resume_rbx.. registers untouched - they
 * already describe the correct outer unwind target and, per the C ABI,
 * still hold the same values they had when this frame started (every
 * function in between must have preserved them as callee-saved regs).
 * Only the program image, stack, and TLS actually change.
 */
int userland_exec_replace(const char* path, int argc, const char* const* argv, const char* const* envp) {
    if (userland_depth == 0) {
        /* Not actually running an existing userland process - there's
         * nothing to "replace in place". Fall back to a fresh exec. */
        return userland_exec(path, argc, argv, envp);
    }

    elf_image_info_t image_info = {0};
    void* entry = elf_load_from_vfs_ex(path, &image_info);
    if (!entry)
        return -1;

    record_exec_context(path, argv);

    map_user_stack();
    userland_heap_init();
    if (init_user_tls(&image_info) != 0) {
        if (image_info.tls_template)
            kfree(image_info.tls_template);
        return -1;
    }
    if (image_info.tls_template)
        kfree(image_info.tls_template);

    uint64_t stack_top = build_initial_user_stack(path, argc, argv, envp, &image_info);

    debug_printf("userland: exec (replace) path=%s entry=%x stack=%x\n",
                 path, entry, stack_top);
    debug_dump_initial_stack(stack_top);

    /* Deliberately NOT touching: userland_depth, userland_frame_stack[],
     * kernel_stack_top, tss.rsp0, userland_saved_kernel_stack_top,
     * userland_saved_tss_rsp0, userland_resume_ret_rip/rsp, or
     * userland_resume_rbx/rbp/r12-r15. Same process, same nesting
     * depth, same eventual unwind target as before this call. */
    asm volatile (
        "cli\n"
        "mov %0, %%r11\n"
        "mov %1, %%r10\n"
        "xor %%rax, %%rax\n"
        "xor %%rbx, %%rbx\n"
        "xor %%rcx, %%rcx\n"
        "xor %%rdx, %%rdx\n"
        "xor %%rsi, %%rsi\n"
        "xor %%rdi, %%rdi\n"
        "xor %%r8, %%r8\n"
        "xor %%r9, %%r9\n"
        "pushq $0x23\n"
        "pushq %%r11\n"
        "pushq $0x202\n"
        "pushq $0x1B\n"
        "pushq %%r10\n"
        "iretq\n"
        :
        : "r"(stack_top), "r"((uint64_t)entry)
        : "memory", "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"
    );

    __builtin_unreachable();
}