#include <stdint.h>
#include <memory.h>
#include <basics.h>
#include <paging.h>
#include <userland.h>
#include <executables/elf.h>
#include <filesystems/vfs.h>

#define LINUX_AT_NULL    0
#define LINUX_AT_PHDR    3
#define LINUX_AT_PHENT   4
#define LINUX_AT_PHNUM   5
#define LINUX_AT_PAGESZ  6
#define LINUX_AT_BASE    7
#define LINUX_AT_FLAGS   8
#define LINUX_AT_ENTRY   9
#define LINUX_AT_UID     11
#define LINUX_AT_EUID    12
#define LINUX_AT_GID     13
#define LINUX_AT_EGID    14
#define LINUX_AT_PLATFORM 15
#define LINUX_AT_HWCAP   16
#define LINUX_AT_HWCAP2  26
#define LINUX_AT_CLKTCK  17
#define LINUX_AT_SECURE  23
#define LINUX_AT_RANDOM  25
#define LINUX_AT_EXECFN  31
#define IA32_FS_BASE_MSR 0xC0000100
#define USER_AUXV_MAX    18

typedef struct {
    int i[4];
} glibc_128bits_t;

typedef union {
    uint64_t counter;
    struct {
        void* val;
        void* to_free;
    } pointer;
} glibc_dtv_t;

typedef struct {
    uint64_t tcb;
    glibc_dtv_t* dtv;
    uint64_t self;
    uint32_t multiple_threads;
    uint32_t gscope_flag;
    uint64_t sysinfo;
    uint64_t stack_guard;
    uint64_t pointer_guard;
    uint64_t unused_vgetcpu_cache[2];
    uint32_t feature_1;
    int32_t __glibc_unused1;
    void* __private_tm[4];
    void* __private_ss;
    uint64_t ssp_base;
    glibc_128bits_t __glibc_unused2[8][4] __attribute__((aligned(32)));
    void* __padding[8];
} glibc_tcb_head_t;

typedef struct {
    glibc_tcb_head_t head;
    glibc_dtv_t dtv[2];
} glibc_tls_block_t;

_Static_assert(__builtin_offsetof(glibc_tcb_head_t, stack_guard) == 0x28, "glibc stack_guard offset mismatch");
_Static_assert(__builtin_offsetof(glibc_tcb_head_t, pointer_guard) == 0x30, "glibc pointer_guard offset mismatch");
_Static_assert(__builtin_offsetof(glibc_tcb_head_t, __private_ss) == 0x70, "glibc __private_ss offset mismatch");
_Static_assert(__builtin_offsetof(glibc_tcb_head_t, __glibc_unused2) == 0x80, "glibc __glibc_unused2 offset mismatch");

typedef struct {
    uint64_t key;
    uint64_t value;
} auxv_pair_t;

static uint64_t user_heap_break = USER_HEAP_VADDR;
static uint64_t user_heap_mapped_end = USER_HEAP_VADDR;

static uint64_t user_mmap_cursor = USER_MMAP_VADDR;
static uint64_t user_mmap_end = USER_MMAP_VADDR;

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

static uint64_t max_u64(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}

static uint64_t push_bytes_to_stack(uint64_t* stack_ptr, const void* src, uint64_t len) {
    *stack_ptr -= len;
    memcpy((void*)*stack_ptr, src, len);
    return *stack_ptr;
}

static uint64_t push_cstr_to_stack(uint64_t* stack_ptr, const char* str) {
    return push_bytes_to_stack(stack_ptr, str, strlen(str) + 1);
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
    "USER=root",
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

void userland_heap_init(void) {
    user_heap_break = USER_HEAP_VADDR;
    user_heap_mapped_end = USER_HEAP_VADDR;

    user_mmap_cursor = USER_MMAP_VADDR;
    user_mmap_end = USER_MMAP_VADDR + USER_MMAP_SIZE;
}

uint64_t userland_brk(uint64_t requested_break) {
    uint64_t heap_end = USER_HEAP_VADDR + USER_HEAP_SIZE;

    if (requested_break == 0) {
        return user_heap_break;
    }

    if (requested_break < USER_HEAP_VADDR || requested_break > heap_end) {
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

int userland_exec(const char* path, int argc, const char* const* argv, const char* const* envp) {
    elf_image_info_t image_info = {0};
    void* entry = elf_load_from_vfs_ex(path, &image_info);
    if (!entry)
        return -1;

    map_user_stack();
    userland_heap_init();
    if (init_user_tls(&image_info) != 0)
        return -1;

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

    return 0;
}
