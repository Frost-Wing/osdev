/**
 * @file kernel.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Main kernel file, everything starts from here
 * @version 0.1
 * @date 2023-10-21
 *
 * @copyright Copyright (c) Pradosh 2023
 *
 */

#include <commands/login.h>
#include <executables/elf.h>
#include <filesystems/vfs.h>
#include <gdt.h>
#include <idt.h>
#include <kernel.h>
#include <klog.h>
#include <multitasking.h>
#include <net/net.h>
#include <ringbuffer.h>
#include <graphics.h>
#include <syslog.h>
#include <tty.h>

int terminal_rows = 0;
int terminal_columns = 0;

uint64 fb_width = 0;
uint64 fb_height = 0;

uint64 *wm_addr;

uint64 *font_address = null;

extern void ksh_exec(void);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent.
static volatile struct limine_framebuffer_request framebuffer_request = {
    LIMINE_FRAMEBUFFER_REQUEST, 0, null};

static volatile struct limine_hhdm_request hhdm_request = {
    LIMINE_HHDM_REQUEST, 0, null};

static volatile struct limine_memmap_request memory_map_request = {
    LIMINE_MEMMAP_REQUEST, 0, null};

static volatile struct limine_smp_request smp_request = {
    LIMINE_SMP_REQUEST, 0, null, 0};

static volatile struct limine_boot_time_request boot_time_request = {
    LIMINE_BOOT_TIME_REQUEST, 0, null};

static volatile struct limine_kernel_file_request kernel_file_request = {
    LIMINE_KERNEL_FILE_REQUEST, 0, null};

struct limine_module_request module_request = {
    LIMINE_MODULE_REQUEST, 0, null, 0, null};

struct flanterm_context *ft_ctx = null;
struct limine_framebuffer *framebuffer = null;
struct memory_context *limine_memory_ctx;

bool isBufferReady = no;

uint32 ctr = 0;

static void ap_entry(struct limine_smp_info *info) {
#if defined(__x86_64__)
    printf("LAPIC ID: 0x%x", info->lapic_id);
#elif defined(__aarch64__)
    printf("GIC CPU Interface no.: 0x%x", info->gic_iface_no);
    printf("MPIDR: 0x%x", info->mpidr);
#elif defined(__riscv)
    printf("Hart ID: 0x%x", info->hartid);
#endif

    __atomic_fetch_add(&ctr, 1, __ATOMIC_SEQ_CST);

    while (1)
        ;
}

#define MOUSE_COLOR_DEFAULT 0xffffffff
#define MOUSE_COLOR_LEFT 0x00ff00ff
#define MOUSE_COLOR_RIGHT 0xff00ff00
#define MOUSE_COLOR_MIDDLE 0xaaaaaaaa

uint32_t mouseColor = MOUSE_COLOR_DEFAULT;

__attribute__((unused)) static void mouseMovementHandler(int64_t xRel, int64_t yRel) {
    (void)xRel;
    (void)yRel;
    ivec2 lastMousePos = GetLastMousePosition();
    ivec2 mousePos = GetMousePosition();

    // glDrawLine((uvec2){0, 0}, (uvec2){lastMousePos.x, lastMousePos.y}, 0x000000);
    // glDrawLine((uvec2){0, 0}, (uvec2){mousePos.x, mousePos.y}, mouseColor);
    print_bitmap((int)lastMousePos.x, (int)lastMousePos.y, 8, 16, mouse_cursor, 0x000000);
    print_bitmap((int)mousePos.x, (int)mousePos.y, 8, 16, mouse_cursor, mouseColor);
}

static char cmdline_value_buf[128];

const char *cmdline_get(const char *cmdline, const char *key) {
    if (!cmdline)
        return null;

    size_t key_len = strlen(key);
    const char *p = cmdline;

    while (*p) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;

        const char *tok_start = p;
        while (*p && *p != ' ')
            p++;
        size_t tok_len = (size_t)(p - tok_start);

        if (tok_len > key_len && tok_start[key_len] == '=' &&
            strncmp(tok_start, key, key_len) == 0) {
            size_t val_len = tok_len - key_len - 1;
            if (val_len >= sizeof(cmdline_value_buf))
                val_len = sizeof(cmdline_value_buf) - 1;
            memcpy(cmdline_value_buf, tok_start + key_len + 1, val_len);
            cmdline_value_buf[val_len] = '\0';
            return cmdline_value_buf;
        }
    }

    return null;
}

__attribute__((unused)) static void mouseButtonHandler(uint8_t button, uint8_t action) {
    if (action == MOUSE_BUTTON_RELEASE) {
        mouseColor = MOUSE_COLOR_DEFAULT;
        return;
    }

    if (button == MOUSE_BUTTON_LEFT) {
        mouseColor = MOUSE_COLOR_LEFT;
        return;
    }

    if (button == MOUSE_BUTTON_RIGHT) {
        mouseColor = MOUSE_COLOR_RIGHT;
        return;
    }

    if (button == MOUSE_BUTTON_MIDDLE) {
        mouseColor = MOUSE_COLOR_MIDDLE;
        return;
    }
}

void main(void) {
    if (framebuffer_request.response == null) {
        hcf2();
    }

    stream_init();
    tty_init();
    keyboard_init();
    // Fetch the first framebuffer.
    framebuffer = framebuffer_request.response->framebuffers[0];
    memmap = memory_map_request.response;
    paging_set_hhdm_offset(hhdm_request.response->offset);

    ft_ctx = flanterm_fb_simple_init(
        framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch);
    isBufferReady = yes;

    if (framebuffer_request.response->framebuffer_count > 1) {
        info("Multiple framebuffers detected! using the first one.", __FILE__);
    }

    terminal_rows = (int)ft_ctx->rows;
    terminal_columns = (int)ft_ctx->cols;
    fb_width = framebuffer->width;
    fb_height = framebuffer->height;

    if (virtualized) { // The code inside this will not work on a real machine.
        probe_serial();
    }

    debug_printf("KERNEL STR -> %z : %z\n", (uint64)virtual_to_physical((uint64_t)(uintptr_t)kstart), kstart);
    debug_printf("KERNEL END -> %z : %z\n", (uint64)virtual_to_physical((uint64_t)(uintptr_t)kend), kend);

    info("Welcome to FrostWing kernel (getting stuff ready)", __FILE__);
    /**
     * ! In memory, kernel is loaded at higher half and at 0x8000000.
     * ! Therefore heap, userland (and more..) can be in the range of 0x1000000 to <= 0x8000000
     */
    mm_init(0x1000000, 64 MiB);

    // Optional method of initializing heap, TODO make an VMM & PMM
    // void* heap_page = allocate_pages(64 MiB / PAGE_SIZE);
    // mm_init(heap_page, 64 MiB);

    limine_memory_ctx = (struct memory_context *)kmalloc(sizeof(struct memory_context));

    acpi_init();

    mm_print_out();

    setup_gdt();
    initIdt();
    klog_init();
    syslog_init();

    RTL8139 = (struct rtl8139 *)kmalloc(sizeof(struct rtl8139));

    analyze_memory_map(limine_memory_ctx, memory_map_request);

    uintptr_t page1 = allocate_page();
    uintptr_t page2 = allocate_page();

    printf("Page1 phys: 0x%x", page1);
    printf("Page2 phys: 0x%x", page2);

    uint64_t *test1 = (uint64_t *)(page1 + hhdm_request.response->offset);
    uint64_t *test2 = (uint64_t *)(page2 + hhdm_request.response->offset);

    // Write some values
    *test1 = 0xDEADBEEFCAFEBABE;
    *test2 = 0x123456789ABCDEF0;

    // Read back
    printf("Read back page1: 0x%x", *test1);
    printf("Read back page2: 0x%x", *test2);

    probe_pci();

    printf(public_key);

    info("Display Resolution: %dx%d (%d) pixels. Pitch: %d", __FILE__ , framebuffer->width, framebuffer->height, framebuffer->width * framebuffer->height, framebuffer->pitch);

    info("Memory Values begin! ===", __FILE__);
    display_memory_formatted(limine_memory_ctx);
    info(reset_color "Memory values end! =====", __FILE__);

    if (limine_memory_ctx->bad != 0) {
        warn("Bad blocks of memory found, it is recommended to replace your RAM.", __FILE__);
    }

    info("Total CPU(s): %d", __FILE__,smp_request.response->cpu_count);
    for (uint64_t i = 0; i < smp_request.response->cpu_count; i++) {
        print_processor_id(i, smp_request.response->cpus[i]->processor_id, smp_request.response->cpus[i]->lapic_id);

        if (smp_request.response->cpus[i]->lapic_id != smp_request.response->bsp_lapic_id) {
            uint32_t old_ctr = __atomic_load_n(&ctr, __ATOMIC_SEQ_CST);

            __atomic_store_n(&smp_request.response->cpus[i]->goto_address, ap_entry, __ATOMIC_SEQ_CST);

            while (__atomic_load_n(&ctr, __ATOMIC_SEQ_CST) == old_ctr);
        }
    }
    print_cpu_info();
    print_L1_cache_info();
    print_L2_cache_info();
    print_L3_cache_info();

    init_rtc();
    display_time();

    enable_fpu();

    check_sse();
    load_complete_sse();

    rtl8139_init(RTL8139);
    ipv4_init(net_ipv4_from_octets(10, 0, 2, 15),
        net_ipv4_from_octets(255, 255, 255, 0),
        net_ipv4_from_octets(10, 0, 2, 2),
        net_ipv4_from_octets(10, 0, 2, 3));

    init_hashing();

    mm_print_out();
    multitasking_init();
    multitasking_start_cursor_blink_task();
    create_user_str("root", "prad");

    const char *cmdline = null;
    if (kernel_file_request.response != null && kernel_file_request.response->kernel_file != null) {
        cmdline = kernel_file_request.response->kernel_file->cmdline;
    } else {
        warn("Limine failed to give command-line data", __FILE__);
    }

    const char *rootdisk = cmdline_get(cmdline, "rootdisk");
    if (rootdisk) {
        info("Mounting root disk from cmdline: %s", __FILE__, rootdisk);
        int ret = vfs_mount(rootdisk, "/", true);
        if (ret != 0) {
            error("Failed to mount root disk '%s' specified via cmdline.", __FILE__, rootdisk);
            hcf2();
        }
        vfs_mount("proc", "/proc", true);
        vfs_mount("dev", "/dev", true);
        vfs_mount("sys", "/sys", true);

    } else {
        warn("No rootdisk= specified on kernel cmdline, root not mounted.", __FILE__, "main");
    }

    enable_fpu();

    info("Welcome to FrostWing Operating System! %s", __FILE__, "(https://github.com/Frost-Wing)");
    frost_compilation_information();

    ksh_exec();
}

void shutdown(void) {
    vfs_sync(true);
    vfs_umount_all(true);
    info("Shutdown has been called", __FILE__);
    acpi_shutdown_hack(hhdm_request.response->offset, acpi_find_sdt);
}

void reboot(void) {
    vfs_sync(true);
    vfs_umount_all(true);
    info("Reboot has been called", __FILE__);
    acpi_reboot(hhdm_request.response->offset);
}
