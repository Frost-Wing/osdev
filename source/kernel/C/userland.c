#include <basics.h>
#include <cc-asm.h>
#include <debugger.h>
#include <executables/elf.h>
#include <filesystems/vfs.h>
#include <heap.h>
#include <memory.h>
#include <paging.h>
#include <stdint.h>
#include <strings.h>
#include <syscalls/internal.h>
#include <tss.h>
#include <tty.h>
#include <userland.h>

#ifdef USERLAND_TRACE
#define utrace(...) printf(__VA_ARGS__)
#else
#define utrace(...) ((void)0)
#endif

#define USERLAND_MAX_DEPTH 8
#define USERLAND_ARGV_MAX 32
#define USERLAND_ENV_MAX 32

static uint64_t user_heap_break = USER_HEAP_VADDR;
static uint64_t user_heap_mapped_end = USER_HEAP_VADDR;
static uint64_t user_mmap_cursor = USER_MMAP_VADDR;
static uint64_t user_mmap_end = USER_MMAP_VADDR + USER_MMAP_SIZE;

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
 * Reentrancy: userland_exec() can be called while an outer userland_exec() is
 * still suspended further down the kernel stack (sh blocks in wait4 ->
 * multitasking_pump() -> userland_exec() for the child). The globals above
 * always describe the innermost frame; outer frames are parked in
 * userland_frame_stack[], each nesting level gets its own syscall stack, and
 * the outer frame's user pages are snapshotted because every frame shares the
 * same user virtual addresses.
 */
typedef struct saved_user_page {
    uint64_t vaddr;
    uint64_t flags;
    uintptr_t data_phys;
    struct saved_user_page *next;
} saved_user_page_t;

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
    uint64_t fs_base;
    uint64_t current_fs_base;
    int last_exit_code;
    uint64_t heap_break;
    uint64_t heap_mapped_end;
    uint64_t mmap_cursor;
    uint64_t mmap_end;
    saved_user_page_t *user_pages;
} userland_saved_frame_t;

typedef struct {
    uint64_t start;
    uint64_t end;
} user_region_t;

/* Code/interp, heap, mmap, TLS and PHDR are contiguous; the stack is separate. */
static const user_region_t user_regions[] = {
    {USER_CODE_VADDR, USER_PHDR_VADDR + USER_PHDR_REGION_SIZE},
    {USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_TOP},
};
#define USER_REGION_COUNT (sizeof(user_regions) / sizeof(user_regions[0]))

static uint64_t userland_restore_fs_base = 0;
static userland_saved_frame_t userland_frame_stack[USERLAND_MAX_DEPTH];
static int userland_depth = 0;

#define USERLAND_SYSCALL_STACK_SIZE 1 MiB

__attribute__((aligned(16)))
static uint8_t userland_syscall_stacks[USERLAND_MAX_DEPTH][USERLAND_SYSCALL_STACK_SIZE];

/* ------------------------------------------------------------ cpu bits --- */

static inline void wrmsr64_local(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr64_local(uint32_t msr) {
    uint32_t low = 0, high = 0;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static uint64_t rdtsc64_local(void) {
    uint32_t lo = 0, hi = 0;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t align_up_u64(uint64_t value, uint64_t align) {
    if (align <= 1)
        return value;
    return (value + align - 1) & ~(align - 1);
}

static uint64_t max_u64(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}

/*
 * Enters ring 3. Zeroes every general register so no kernel values leak.
 * The operands are consumed before rbp/r12-r15 are zeroed.
 */
static inline __attribute__((noreturn)) void userland_iret(uint64_t entry, uint64_t stack) {
    register uint64_t r_stack asm("r11") = stack;
    register uint64_t r_entry asm("r10") = entry;

    asm volatile(
        "cli\n"
        "xor %%eax, %%eax\n"
        "xor %%ebx, %%ebx\n"
        "xor %%ecx, %%ecx\n"
        "xor %%edx, %%edx\n"
        "xor %%esi, %%esi\n"
        "xor %%edi, %%edi\n"
        "xor %%r8d, %%r8d\n"
        "xor %%r9d, %%r9d\n"
        "xor %%r12d, %%r12d\n"
        "xor %%r13d, %%r13d\n"
        "xor %%r14d, %%r14d\n"
        "xor %%r15d, %%r15d\n"
        "xor %%ebp, %%ebp\n"
        "pushq $0x23\n"
        "pushq %%r11\n"
        "pushq $0x202\n"
        "pushq $0x1B\n"
        "pushq %%r10\n"
        "iretq\n"
        :
        : "r"(r_stack), "r"(r_entry)
        : "memory", "rax", "rbx", "rcx", "rdx", "rsi", "rdi",
          "r8", "r9", "r12", "r13", "r14", "r15");
    __builtin_unreachable();
}
/* Implemented in assembly: it restores RBP/RSP and never returns to C. */
extern void userland_jump_resume(const uint64_t *regs) __attribute__((noreturn));

/* -------------------------------------------------------- page mapping --- */

static void unmap_user_range(uint64_t start, uint64_t end) {
    uint64_t a = start & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t b = (end + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

    for (uint64_t vaddr = a; vaddr < b; vaddr += PAGE_SIZE)
        unmap_user_page(vaddr);
}

/* Maps zeroed pages. On failure everything mapped by this call is undone. */
static bool map_user_range(uint64_t start, uint64_t end, uint64_t flags) {
    uint64_t a = start & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t b = (end + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

    for (uint64_t vaddr = a; vaddr < b; vaddr += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        if (!phys) {
            eprintf("userland: out of memory mapping %x-%x", start, end);
            unmap_user_range(a, vaddr);
            return false;
        }
        if (paging_user_page_flags(vaddr) & PAGE_PRESENT)
            unmap_user_page(vaddr);
        map_user_page(vaddr, phys, flags);
        memset((void *)vaddr, 0, PAGE_SIZE);
    }
    return true;
}

static bool map_user_stack(void) {
    return map_user_range(USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_TOP, USER_DATA_FLAGS);
}

static void userland_unmap_all(void) {
    for (size_t i = 0; i < USER_REGION_COUNT; i++)
        unmap_user_range(user_regions[i].start, user_regions[i].end);
}

/* ----------------------------------------------------------- snapshots --- */

static void userland_free_snapshot(saved_user_page_t *pages) {
    while (pages) {
        saved_user_page_t *next = pages->next;
        free_page(pages->data_phys);
        kfree(pages);
        pages = next;
    }
}

static bool userland_snapshot_range(saved_user_page_t **list, uint64_t start, uint64_t end) {
    for (uint64_t vaddr = start; vaddr < end; vaddr += PAGE_SIZE) {
        uint64_t flags = paging_user_page_flags(vaddr);
        if (!(flags & PAGE_PRESENT))
            continue;

        saved_user_page_t *page = (saved_user_page_t *)kmalloc(sizeof(saved_user_page_t));
        if (!page)
            return false;

        page->data_phys = allocate_page();
        if (!page->data_phys) {
            kfree(page);
            return false;
        }
        page->vaddr = vaddr;
        page->flags = flags;
        memcpy(paging_phys_to_virt(page->data_phys), (const void *)vaddr,
            PAGE_SIZE);
        page->next = *list;
        *list = page;
    }
    return true;
}

/* All-or-nothing: a partial snapshot would corrupt the process on restore. */
static bool userland_snapshot_mappings(saved_user_page_t **out) {
    saved_user_page_t *pages = NULL;

    for (size_t i = 0; i < USER_REGION_COUNT; i++) {
        if (!userland_snapshot_range(&pages, user_regions[i].start, user_regions[i].end)) {
            userland_free_snapshot(pages);
            return false;
        }
    }

    *out = pages;
    return true;
}

static void userland_restore_snapshot(saved_user_page_t *pages) {
    userland_unmap_all();

    for (saved_user_page_t *page = pages; page; page = page->next) {
        uint64_t phys = allocate_page();
        if (!phys) {
            eprintf("userland: out of memory restoring %x", page->vaddr);
            continue;
        }
        map_user_page(page->vaddr, phys, page->flags);
        memcpy(paging_phys_to_virt(phys), paging_phys_to_virt(page->data_phys), PAGE_SIZE);
    }
}

/* -------------------------------------------------------------- frames --- */

static int userland_push_frame(bool fresh_space) {
    if (userland_depth >= USERLAND_MAX_DEPTH)
        return -1;

    saved_user_page_t *pages = NULL;
    if (userland_depth > 0 && !userland_snapshot_mappings(&pages)) {
        eprintf("[userland] out of memory snapshotting parent");
        return -1;
    }

    userland_saved_frame_t *f = &userland_frame_stack[userland_depth];
    f->saved_kernel_stack_top = userland_saved_kernel_stack_top;
    f->saved_tss_rsp0 = userland_saved_tss_rsp0;
    f->resume_rip = userland_resume_rip;
    f->resume_rsp = userland_resume_rsp;
    f->resume_rbx = userland_resume_rbx;
    f->resume_rbp = userland_resume_rbp;
    f->resume_r12 = userland_resume_r12;
    f->resume_r13 = userland_resume_r13;
    f->resume_r14 = userland_resume_r14;
    f->resume_r15 = userland_resume_r15;
    f->resume_ret_rip = userland_resume_ret_rip;
    f->resume_ret_rsp = userland_resume_ret_rsp;
    f->fs_base = rdmsr64_local(IA32_FS_BASE_MSR);
    f->current_fs_base = current_fs_base;
    f->last_exit_code = userland_last_exit_code;
    f->heap_break = user_heap_break;
    f->heap_mapped_end = user_heap_mapped_end;
    f->mmap_cursor = user_mmap_cursor;
    f->mmap_end = user_mmap_end;
    f->user_pages = pages;

    int index = userland_depth++;

    /* The child gets a clean address space; the parent's pages live in the snapshot. */
    if (index > 0 && fresh_space)
        userland_unmap_all();

    return index;
}

/* Returns true if an outer frame is still active. */
static bool userland_pop_frame(void) {
    if (userland_depth == 0)
        return false;

    userland_depth--;

    if (userland_depth == 0) {
        userland_free_snapshot(userland_frame_stack[0].user_pages);
        userland_frame_stack[0].user_pages = NULL;
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
        userland_restore_fs_base = 0;
        current_fs_base = 0;
        return false;
    }

    /* Slot [depth] holds the outer frame that was active when this one started. */
    userland_saved_frame_t *f = &userland_frame_stack[userland_depth];
    userland_saved_kernel_stack_top = f->saved_kernel_stack_top;
    userland_saved_tss_rsp0 = f->saved_tss_rsp0;
    userland_resume_rip = f->resume_rip;
    userland_resume_rsp = f->resume_rsp;
    userland_resume_rbx = f->resume_rbx;
    userland_resume_rbp = f->resume_rbp;
    userland_resume_r12 = f->resume_r12;
    userland_resume_r13 = f->resume_r13;
    userland_resume_r14 = f->resume_r14;
    userland_resume_r15 = f->resume_r15;
    userland_resume_ret_rip = f->resume_ret_rip;
    userland_resume_ret_rsp = f->resume_ret_rsp;
    userland_restore_fs_base = f->fs_base;
    current_fs_base = f->current_fs_base;
    userland_last_exit_code = f->last_exit_code;
    user_heap_break = f->heap_break;
    user_heap_mapped_end = f->heap_mapped_end;
    user_mmap_cursor = f->mmap_cursor;
    user_mmap_end = f->mmap_end;

    userland_restore_snapshot(f->user_pages);
    userland_free_snapshot(f->user_pages);
    f->user_pages = NULL;
    return true;
}

__attribute__((noinline, noreturn)) static void userland_finish_exit(void) {
    int exit_code = userland_last_exit_code;

    utrace("finish_exit: depth=%d code=%d ret_rip=%x ret_rsp=%x\n",
        userland_depth, exit_code, userland_resume_ret_rip, userland_resume_ret_rsp);

    /* Snapshot the caller-register state BEFORE popping overwrites it. */
    uint64_t regs[9] = {
        userland_resume_rbx,
        userland_resume_rbp,
        userland_resume_r12,
        userland_resume_r13,
        userland_resume_r14,
        userland_resume_r15,
        (uint64_t)(uint32_t)exit_code,
        userland_resume_ret_rsp,
        userland_resume_ret_rip,
    };

    // debug_printf("[userland] finish depth=%u exit=%u ret_rsp=%u ret_rip=%u resume_rsp=%u resume_rip=%u\n", (uint32_t)userland_depth, (uint32_t)exit_code, (uint32_t)userland_resume_ret_rsp, (uint32_t)userland_resume_ret_rip, (uint32_t)userland_resume_rsp, (uint32_t)userland_resume_rip);

    userland_should_return_kernel = false;

    kernel_stack_top = userland_saved_kernel_stack_top;
    tss.rsp0 = userland_saved_tss_rsp0;

    bool still_in_userland = userland_pop_frame();
    userland_running = still_in_userland;

    wrmsr64_local(IA32_FS_BASE_MSR, userland_restore_fs_base);
    current_fs_base = userland_restore_fs_base;

    if (!still_in_userland) {
        sys_rseq_reset_current();
        userland_unmap_all();
        userland_heap_init();
    }

    if (!still_in_userland)
       tty_flush_input();
    // printf(blue_color "\n[process exited with code %d]" reset_color, exit_code);
    asm volatile("sti");

    userland_jump_resume(regs);
}

/* --------------------------------------------------------- user stack ---- */

static uint64_t push_bytes_to_stack(uint64_t *stack_ptr, const void *src, uint64_t len) {
    *stack_ptr -= len;
    memcpy((void *)*stack_ptr, src, len);
    return *stack_ptr;
}

static uint64_t push_cstr_to_stack(uint64_t *stack_ptr, const char *str) {
    return push_bytes_to_stack(stack_ptr, str, (uint64_t)strlen(str) + 1U);
}

static int string_array_count(const char *const *arr) {
    int count = 0;
    while (arr && arr[count])
        count++;
    return count;
}

static const char *const default_envp[] = {
    "HOME=/",
    "PATH=/",
    "TERM=linux",
    "USER=none",
    "SHLVL=1",
    NULL};

_Static_assert(USER_AUXV_MAX >= 18, "auxv table too small");

/* Returns the initial rsp, or 0 if argv/envp do not fit in the user stack. */
static uint64_t build_initial_user_stack(const char *exec_path,
    int argc,
    const char *const *argv,
    const char *const *envp,
    const elf_image_info_t *image_info) {
    const char *const *final_envp = envp ? envp : default_envp;
    int envc = string_array_count(final_envp);
    int argvc = argc;
    uint64_t stack_ptr = USER_STACK_TOP;
    uint64_t argv_addrs[USERLAND_ARGV_MAX];
    uint64_t env_addrs[USERLAND_ENV_MAX];
    uint8_t random_bytes[16];
    char exec_name_buf[256];
    auxv_pair_t auxv[USER_AUXV_MAX];
    int auxc = 0;

    if (argvc < 0)
        argvc = 0;
    if (argvc > USERLAND_ARGV_MAX)
        argvc = USERLAND_ARGV_MAX;
    if (envc > USERLAND_ENV_MAX)
        envc = USERLAND_ENV_MAX;

    snprintf(exec_name_buf, sizeof(exec_name_buf), "%s", exec_path ? exec_path : "");

    /* Measure first so nothing is written below the mapped stack. */
    uint64_t need = sizeof(random_bytes) + sizeof("x86_64") + strlen(exec_name_buf) + 1;
    for (int i = 0; i < envc; ++i)
        need += strlen(final_envp[i] ? final_envp[i] : "") + 1;
    for (int i = 0; i < argvc; ++i)
        need += strlen((argv && argv[i]) ? argv[i] : "") + 1;
    need += (uint64_t)(4 + argvc + envc + 2 * (USER_AUXV_MAX + 1)) * sizeof(uint64_t) + 64;
    if (need > USER_STACK_SIZE / 2) {
        eprintf("userland: argv/envp too large (%u bytes)", (uint32_t)need);
        return 0;
    }

    for (int i = 0; i < (int)sizeof(random_bytes); ++i)
        random_bytes[i] = (uint8_t)(rdtsc64_local() >> ((i & 7) * 8));
    uint64_t random_addr = push_bytes_to_stack(&stack_ptr, random_bytes, sizeof(random_bytes));
    uint64_t platform_addr = push_cstr_to_stack(&stack_ptr, "x86_64");
    uint64_t execfn_addr = push_cstr_to_stack(&stack_ptr, exec_name_buf);

    for (int i = envc - 1; i >= 0; --i)
        env_addrs[i] = push_cstr_to_stack(&stack_ptr, final_envp[i] ? final_envp[i] : "");

    for (int i = argvc - 1; i >= 0; --i)
        argv_addrs[i] = push_cstr_to_stack(&stack_ptr, (argv && argv[i]) ? argv[i] : "");

    auxv[auxc++] = (auxv_pair_t){LINUX_AT_PHDR, image_info ? image_info->phdr_addr : 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_PHENT, image_info ? image_info->phentsize : 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_PHNUM, image_info ? image_info->phnum : 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_PAGESZ, PAGE_SIZE};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_BASE, image_info ? image_info->interp_base : 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_FLAGS, 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_ENTRY, image_info ? image_info->entry : 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_UID, 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_EUID, 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_GID, 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_EGID, 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_HWCAP, 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_CLKTCK, 100};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_SECURE, 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_RANDOM, random_addr};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_HWCAP2, 0};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_PLATFORM, platform_addr};
    auxv[auxc++] = (auxv_pair_t){LINUX_AT_EXECFN, execfn_addr};

    /* argc, argv[], NULL, envp[], NULL, auxv[], AT_NULL */
    uint64_t frame_words = 1 + (uint64_t)argvc + 1 + (uint64_t)envc + 1 + ((uint64_t)(auxc + 1) * 2);
    uint64_t frame_ptr = (stack_ptr - frame_words * sizeof(uint64_t)) & ~0xFULL; /* ABI: 16-byte aligned at entry */

    uint64_t *out = (uint64_t *)frame_ptr;
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

/* ----------------------------------------------------------------- TLS --- */

static int init_user_tls(const elf_image_info_t *image_info) {
    uint64_t tls_memsz = image_info ? image_info->tls_memsz : 0;
    uint64_t tls_filesz = image_info ? image_info->tls_filesz : 0;
    uint64_t tls_align = (image_info && image_info->tls_align) ? image_info->tls_align : 1;

    if ((tls_align & (tls_align - 1)) != 0)
        tls_align = 1;

    if (tls_filesz > tls_memsz || tls_memsz > USER_TLS_REGION_SIZE || tls_align > PAGE_SIZE) {
        eprintf("userland: invalid TLS memsz=%x filesz=%x align=%x", tls_memsz, tls_filesz, tls_align);
        return -1;
    }

    /* The TLS block sits directly below the TCB, so the TCB needs the block's alignment too. */
    uint64_t tls_block_size = tls_memsz ? align_up_u64(tls_memsz, tls_align) : 0;
    uint64_t tcb_addr = align_up_u64(USER_TLS_VADDR + tls_block_size, max_u64(16, tls_align));
    uint64_t tls_block_addr = tls_block_size ? (tcb_addr - tls_block_size) : tcb_addr;
    uint64_t tls_end = align_up_u64(tcb_addr + sizeof(glibc_tls_block_t), PAGE_SIZE);

    if (tls_end > USER_TLS_VADDR + USER_TLS_REGION_SIZE) {
        eprintf("userland: TLS region too small end=%x limit=%x",
            tls_end, USER_TLS_VADDR + USER_TLS_REGION_SIZE);
        return -1;
    }

    if (!map_user_range(USER_TLS_VADDR, tls_end, USER_DATA_FLAGS))
        return -1;

    uint64_t guard = (rdtsc64_local() ^ 0x9e3779b97f4a7c15ULL) & ~0xFFULL;

    if (tls_filesz && image_info->tls_template)
        memcpy((void *)tls_block_addr, image_info->tls_template, tls_filesz);

    glibc_tls_block_t *tls = (glibc_tls_block_t *)tcb_addr;
    glibc_tcb_head_t *tcb = &tls->head;

    tls->dtv[0].counter = 1;
    tls->dtv[1].pointer.val = (void *)tls_block_addr;
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

    wrmsr64_local(IA32_FS_BASE_MSR, tcb_addr);
    current_fs_base = tcb_addr;
    return 0;
}

/* ------------------------------------------------------ heap and mmap ---- */

void userland_heap_init(void) {
    user_heap_break = USER_HEAP_VADDR;
    user_heap_mapped_end = USER_HEAP_VADDR;
    user_mmap_cursor = USER_MMAP_VADDR;
    user_mmap_end = USER_MMAP_VADDR + USER_MMAP_SIZE;
}

uint64_t userland_brk(uint64_t requested_break) {
    uint64_t user_heap_end = USER_HEAP_VADDR + USER_HEAP_SIZE;

    if (requested_break == 0)
        return user_heap_break;

    if (requested_break < USER_HEAP_VADDR || requested_break > user_heap_end)
        return user_heap_break;

    if (requested_break > user_heap_mapped_end) {
        if (!map_user_range(user_heap_mapped_end, requested_break, USER_DATA_FLAGS))
            return user_heap_break;
        user_heap_mapped_end = (requested_break + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    } else if (requested_break < user_heap_break) {
        /* Pages stay mapped after a shrink, but must read as zero when regrown. */
        memset((void *)requested_break, 0, user_heap_break - requested_break);
    }

    user_heap_break = requested_break;
    return user_heap_break;
}

uint64_t userland_mmap_fixed(uint64_t addr, uint64_t length) {
    if (addr == 0 || length == 0)
        return 0;

    /* Only inside the regions that are snapshotted and unmapped on exit. */
    if (addr < USER_CODE_VADDR || addr >= USER_TLS_VADDR || length > USER_TLS_VADDR - addr)
        return 0;

    if (!map_user_range(addr, addr + length, USER_DATA_FLAGS))
        return 0;

    return addr;
}

bool userland_mprotect(uint64_t addr, uint64_t length, uint64_t prot) {
    if (length == 0)
        return true;

    uint64_t end = addr + length;
    if (end < addr || (addr & (PAGE_SIZE - 1)) ||
        (prot & ~(LINUX_PROT_READ | LINUX_PROT_WRITE | LINUX_PROT_EXEC)))
        return false;

    uint64_t aligned_end = (end + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    if (aligned_end < end ||
        addr < USER_CODE_VADDR ||
        aligned_end > USER_PHDR_VADDR + USER_PHDR_REGION_SIZE)
        return false;

    for (uint64_t page = addr; page < aligned_end; page += PAGE_SIZE) {
        if (!(paging_user_page_flags(page) & PAGE_PRESENT))
            return false;
    }

    bool writable = (prot & LINUX_PROT_WRITE) != 0;
    bool executable = (prot & LINUX_PROT_EXEC) != 0;
    for (uint64_t page = addr; page < aligned_end; page += PAGE_SIZE) {
        if (!paging_set_user_page_permissions(page, writable, executable))
            return false;
    }
    return true;
}

uint64_t userland_mmap_anon(uint64_t length) {
    if (length == 0 || length > UINT64_MAX - (PAGE_SIZE - 1))
        return 0;

    uint64_t aligned_len = (length + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    if (aligned_len > user_mmap_end - USER_MMAP_VADDR)
        return 0;

    uint64_t pages = aligned_len / PAGE_SIZE;
    uint64_t candidates[2] = {
        user_mmap_cursor < user_mmap_end ? user_mmap_cursor : USER_MMAP_VADDR,
        USER_MMAP_VADDR,
    };

    for (size_t pass = 0; pass < 2; ++pass) {
        uint64_t first = candidates[pass];
        uint64_t limit = pass == 0 ? user_mmap_end : candidates[0];
        if (first < USER_MMAP_VADDR)
            first = USER_MMAP_VADDR;
        first = (first + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
        if (limit < first || limit - first < aligned_len)
            continue;

        for (uint64_t base = first; base <= limit - aligned_len; base += PAGE_SIZE) {
            uint64_t page = 0;
            while (page < pages &&
                !(paging_user_page_flags(base + page * PAGE_SIZE) & PAGE_PRESENT))
                ++page;
            if (page != pages) {
                base += page * PAGE_SIZE;
                continue;
            }

            if (!map_user_range(base, base + aligned_len, USER_DATA_FLAGS))
                return 0;
            user_mmap_cursor = base + aligned_len;
            if (user_mmap_cursor >= user_mmap_end)
                user_mmap_cursor = USER_MMAP_VADDR;
            return base;
        }
    }

    return 0;
}

bool userland_mmap_unmap(uint64_t addr, uint64_t length) {
    if (length == 0 || addr < USER_MMAP_VADDR || addr >= user_mmap_end ||
        length > user_mmap_end - addr)
        return false;

    uint64_t end = addr + length;
    unmap_user_range(addr, end);
    if (addr < user_mmap_cursor)
        user_mmap_cursor = addr;
    return true;
}

/* ------------------------------------------------------ exit and faults --- */

bool userland_prepare_exit(syscall_frame_t *frame, uint64_t exit_code) {
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
        case 0:
            return 136;
        case 6:
            return 132;
        case 13:
        case 14:
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

    uint64_t regs[9] = {
        userland_resume_rbx,
        userland_resume_rbp,
        userland_resume_r12,
        userland_resume_r13,
        userland_resume_r14,
        userland_resume_r15,
        0,
        userland_resume_rsp,
        userland_resume_rip,
    };

    asm volatile("cli");
    userland_jump_resume(regs);
}

/* --------------------------------------------------------------- entry --- */

void enter_userland_at(uint64_t code_entry) {
    if (!map_user_stack())
        return;

    userland_heap_init();
    if (init_user_tls(NULL) != 0)
        return;

    userland_iret(code_entry, USER_STACK_TOP);
}

void userland_exec_prepare(
    const char *path,
    int argc,
    const char *argv[],
    elf_image_info_t *out_info,
    void **out_entry,
    uint64_t *out_stack) {
    elf_image_info_t image_info = {0};
    void *entry = elf_load_from_vfs_ex(path, &image_info);

    if (entry && !map_user_stack())
        entry = NULL;

    if (!entry) {
        if (image_info.tls_template)
            kfree(image_info.tls_template);
        if (out_entry)
            *out_entry = NULL;
        if (out_stack)
            *out_stack = 0;
        return;
    }

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

static bool userland_path_exists(const char *path) {
    vfs_file_t file;
    if (vfs_open(path, VFS_RDONLY, &file) != 0)
        return false;
    vfs_close(&file);
    return true;
}

/* Undo a half-finished exec after push_frame(). Always returns -1. */
static int userland_exec_fail(elf_image_info_t *info) {
    if (info && info->tls_template) {
        kfree(info->tls_template);
        info->tls_template = NULL;
    }

    bool still_in_userland = userland_pop_frame();
    if (!still_in_userland) {
        userland_unmap_all();
        userland_heap_init();
    }

    wrmsr64_local(IA32_FS_BASE_MSR, userland_restore_fs_base);
    current_fs_base = userland_restore_fs_base;
    return -1;
}

__attribute__((noinline, used))
int userland_exec_impl(const userland_exec_ctx_t *ctx, const userland_caller_state_t *caller) {
    if (!ctx || !ctx->path || !caller)
        return -1;

    // debug_printf("[userland] exec caller=%u depth=%u ret_rsp=%u ret_rip=%u rbp=%u rbx=%u r12=%u r13=%u r14=%u r15=%u\n", (uint32_t)(uintptr_t)caller, (uint32_t)userland_depth, (uint32_t)caller->ret_rsp, (uint32_t)caller->ret_rip, (uint32_t)caller->rbp, (uint32_t)caller->rbx, (uint32_t)caller->r12, (uint32_t)caller->r13, (uint32_t)caller->r14, (uint32_t)caller->r15);

    /* Cheap ENOENT check before any snapshot/unmap work. */
    if (!userland_path_exists(ctx->path))
        return -1;

    /* push_frame() FIRST: it captures the outer frame's globals and address space. */
    int frame_depth = userland_push_frame(true);
    if (frame_depth < 0) {
        eprintf("[userland] exec nesting too deep or out of memory");
        return -1;
    }

    userland_resume_ret_rip = caller->ret_rip;
    userland_resume_ret_rsp = caller->ret_rsp;
    userland_resume_rbx = caller->rbx;
    userland_resume_rbp = caller->rbp;
    userland_resume_r12 = caller->r12;
    userland_resume_r13 = caller->r13;
    userland_resume_r14 = caller->r14;
    userland_resume_r15 = caller->r15;

    elf_image_info_t image_info = {0};
    void *entry = elf_load_from_vfs_ex(ctx->path, &image_info);
    if (!entry)
        return userland_exec_fail(&image_info);

    if (!map_user_stack())
        return userland_exec_fail(&image_info);

    const char *safe_argv[USERLAND_ARGV_MAX];
    int safe_argc = ctx->argc;
    if (safe_argc < 0)
        safe_argc = 0;
    if (safe_argc > USERLAND_ARGV_MAX - 1)
        safe_argc = USERLAND_ARGV_MAX - 1;
    for (int i = 0; i < safe_argc; i++)
        safe_argv[i] = ctx->argv[i] ? ctx->argv[i] : "";
    safe_argv[safe_argc] = NULL;

    uint64_t stack_top = build_initial_user_stack(ctx->path, safe_argc, safe_argv, ctx->envp, &image_info);
    if (!stack_top)
        return userland_exec_fail(&image_info);

    /* Last fallible step: it writes FS_BASE only once it can no longer fail. */
    if (init_user_tls(&image_info) != 0)
        return userland_exec_fail(&image_info);

    if (image_info.tls_template) {
        kfree(image_info.tls_template);
        image_info.tls_template = NULL;
    }

    sys_rseq_reset_current();
    userland_heap_init();

    uint64_t kernel_rsp = 0;
    asm volatile("mov %%rsp, %0" : "=r"(kernel_rsp));

    /*
     * finish_exit/abort enter userland_finish_exit with jmp, not call, so the
     * saved rsp must look like a post-call rsp (16n + 8). The slot below
     * kernel_rsp is free: this function never returns normally from here on.
     */
    userland_resume_rsp = (kernel_rsp & ~0xFULL) - 8;
    userland_resume_rip = (uint64_t)userland_finish_exit;

    userland_should_return_kernel = false;
    userland_last_exit_code = 0;
    userland_running = true;

    userland_saved_kernel_stack_top = kernel_stack_top;
    userland_saved_tss_rsp0 = tss.rsp0;

    kernel_stack_top = (uint64_t)&userland_syscall_stacks[frame_depth][sizeof(userland_syscall_stacks[frame_depth])];
    tss.rsp0 = kernel_stack_top;

    userland_iret((uint64_t)entry, stack_top);
}

__attribute__((noinline, used))
int userland_fork_impl(const userland_regs_t *regs, const userland_caller_state_t *caller) {
    if (!regs || !caller || userland_depth == 0)
        return USERLAND_FORK_FAILED;

    int frame_depth = userland_push_frame(false);
    if (frame_depth < 0) {
        eprintf("[userland] fork nesting too deep or out of memory");
        return USERLAND_FORK_FAILED;
    }

    userland_resume_ret_rip = caller->ret_rip;
    userland_resume_ret_rsp = caller->ret_rsp;
    userland_resume_rbx = caller->rbx;
    userland_resume_rbp = caller->rbp;
    userland_resume_r12 = caller->r12;
    userland_resume_r13 = caller->r13;
    userland_resume_r14 = caller->r14;
    userland_resume_r15 = caller->r15;

    uint64_t kernel_rsp = 0;
    asm volatile("mov %%rsp, %0" : "=r"(kernel_rsp));
    userland_resume_rsp = (kernel_rsp & ~0xFULL) - 8;
    userland_resume_rip = (uint64_t)userland_finish_exit;

    userland_should_return_kernel = false;
    userland_last_exit_code = 0;
    userland_running = true;

    userland_saved_kernel_stack_top = kernel_stack_top;
    userland_saved_tss_rsp0 = tss.rsp0;

    kernel_stack_top = (uint64_t)&userland_syscall_stacks[frame_depth][sizeof(userland_syscall_stacks[frame_depth])];
    tss.rsp0 = kernel_stack_top;

    userland_iret_regs(regs);
}

/* ------------------------------------------------------ execve (replace) -- */

typedef struct {
    userland_exec_ctx_t ctx;
    const char *envv[USERLAND_ENV_MAX + 1];
    char *owned[1 + USERLAND_ARGV_MAX + USERLAND_ENV_MAX];
    int owned_count;
} exec_copy_t;

static const char *exec_copy_str(exec_copy_t *c, const char *s) {
    char *d = strdup(s ? s : "");
    if (d)
        c->owned[c->owned_count++] = d;
    return d;
}

static void exec_copy_free(exec_copy_t *c) {
    for (int i = 0; i < c->owned_count; i++)
        kfree(c->owned[i]);
    kfree(c);
}

/* Deep copy: the old user image (where the strings may live) is about to be unmapped. */
static exec_copy_t *exec_copy_create(const userland_exec_ctx_t *src) {
    exec_copy_t *c = (exec_copy_t *)kmalloc(sizeof(*c));
    if (!c)
        return NULL;
    memset(c, 0, sizeof(*c));

    int argc = src->argc;
    if (argc < 0)
        argc = 0;
    if (argc > USERLAND_ARGV_MAX - 1)
        argc = USERLAND_ARGV_MAX - 1;
    c->ctx.argc = argc;

    bool ok = (c->ctx.path = exec_copy_str(c, src->path)) != NULL;

    for (int i = 0; ok && i < argc; i++)
        ok = (c->ctx.argv[i] = exec_copy_str(c, src->argv[i])) != NULL;

    if (ok && src->envp) {
        int n = 0;
        for (; ok && n < USERLAND_ENV_MAX && src->envp[n]; n++)
            ok = (c->envv[n] = exec_copy_str(c, src->envp[n])) != NULL;
        c->envv[n] = NULL;
        c->ctx.envp = c->envv;
    }

    if (!ok) {
        exec_copy_free(c);
        return NULL;
    }
    return c;
}

/*
 * execve() for an already running process: same resume identity, new image.
 * The old image is snapshotted so a failed exec returns -1 to a working process.
 */
int userland_exec_replace(const userland_exec_ctx_t *ctx) {
    if (!ctx || !ctx->path)
        return -1;

    if (userland_depth == 0)
        return userland_exec(ctx);

    exec_copy_t *copy = exec_copy_create(ctx);
    if (!copy)
        return -1;
    const userland_exec_ctx_t *c = &copy->ctx;

    if (!userland_path_exists(c->path)) {
        exec_copy_free(copy);
        return -1;
    }

    saved_user_page_t *backup = NULL;
    if (!userland_snapshot_mappings(&backup)) {
        exec_copy_free(copy);
        return -1;
    }

    userland_unmap_all();

    elf_image_info_t image_info = {0};
    void *entry = elf_load_from_vfs_ex(c->path, &image_info);
    uint64_t stack_top = 0;

    if (entry && map_user_stack())
        stack_top = build_initial_user_stack(c->path, c->argc, c->argv, c->envp, &image_info);

    if (stack_top && init_user_tls(&image_info) == 0) {
        if (image_info.tls_template)
            kfree(image_info.tls_template);
        userland_free_snapshot(backup);

        exec_copy_free(copy);
        sys_rseq_reset_current();
        userland_heap_init();
        userland_iret((uint64_t)entry, stack_top);
    }

    /* Failed after the point of no return: put the old image back. */
    if (image_info.tls_template)
        kfree(image_info.tls_template);
    userland_restore_snapshot(backup);
    userland_free_snapshot(backup);
    exec_copy_free(copy);
    return -1;
}
