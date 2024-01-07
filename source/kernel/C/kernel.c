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
#include <kernel.h>
#define SSFN_CONSOLEBITMAP_TRUECOLOR
#define SSFN_memcmp  memcmp
#define SSFN_memset  memset
#define SSFN_realloc realloc
#define SSFN_free    free
#include <fonts/ssfn.h>

int terminal_rows = 0;
int terminal_columns = 0;

int64 fb_width = 0;
int64 fb_height = 0;

/**
 * @brief Assert Definition
 * @authors GAMINGNOOB (Coded Original) & Pradosh (Modified it)
 */
#define assert(expression, file, line) if(!(expression)){printf("\x1b[31mAssert Failed! at \x1b[36m%s:%d\x1b[0m => \x1b[32m%s\x1b[0m", file, line, #expression); hcf();}

extern int typescript_main(void);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent.
static volatile struct limine_framebuffer_request framebuffer_request = {
    LIMINE_FRAMEBUFFER_REQUEST, 0, null
};

static volatile struct limine_hhdm_request hhdm_request = {
    LIMINE_HHDM_REQUEST, 0, null
};

static volatile struct limine_memmap_request memory_map_request = {
    LIMINE_MEMMAP_REQUEST, 0, null
};

static volatile struct limine_smp_request smp_request = {
    LIMINE_SMP_REQUEST, 0, null, 0
};

static volatile struct limine_boot_time_request boot_time_request = {
    LIMINE_BOOT_TIME_REQUEST, 0, null
};

struct limine_module_request module_request = {
    LIMINE_MODULE_REQUEST, 0, null
};

struct memory_context {
    int64 total;
    int64 usable;
    int64 reserved;
    int64 acpi_reclaimable;
    int64 acpi_nvs;
    int64 bad;
    int64 bootloader_reclaimable;
    int64 kernel_modules;
    int64 framebuffer;            // Mostly unneeded because frame buffer struct separately gives it,
    int64 unknown;                // This value must be always 0.
};


struct flanterm_context *ft_ctx = null;
struct limine_framebuffer *framebuffer = null;

bool isBufferReady = no;
// bool logoBoot = no;

int32 ctr = 0;

void ap_entry(struct limine_smp_info *info) {
#if defined (__x86_64__)
    printf("LAPIC ID: 0x%x", info->lapic_id);
#elif defined (__aarch64__)
    printf("GIC CPU Interface no.: 0x%x", info->gic_iface_no);
    printf("MPIDR: 0x%x", info->mpidr);
#elif defined (__riscv)
    printf("Hart ID: 0x%x", info->hartid);
#endif

    __atomic_fetch_add(&ctr, 1, __ATOMIC_SEQ_CST);

    while (1);
}

#define MOUSE_COLOR_DEFAULT 0xffffffff
#define MOUSE_COLOR_LEFT 0x00ff00ff
#define MOUSE_COLOR_RIGHT 0xff00ff00
#define MOUSE_COLOR_MIDDLE 0xaaaaaaaa

uint32_t mouseColor = MOUSE_COLOR_DEFAULT;

void mouseMovementHandler(int64_t xRel, int64_t yRel)
{
    ivec2 lastMousePos = GetLastMousePosition();
    ivec2 mousePos = GetMousePosition();

    // glDrawLine((uvec2){0, 0}, (uvec2){lastMousePos.x, lastMousePos.y}, 0x000000);
    // glDrawLine((uvec2){0, 0}, (uvec2){mousePos.x, mousePos.y}, mouseColor);
    print_bitmap(lastMousePos.x, lastMousePos.y, 8, 16, mouse_cursor, 0x000000);
    print_bitmap(mousePos.x, mousePos.y, 8, 16, mouse_cursor, mouseColor);
}

void mouseButtonHandler(uint8_t button, uint8_t action)
{
    if (action == MOUSE_BUTTON_RELEASE)
    {
        mouseColor = MOUSE_COLOR_DEFAULT;
        return;
    }

    if (button == MOUSE_BUTTON_LEFT)
    {
        mouseColor = MOUSE_COLOR_LEFT;
        return;
    }

    if (button == MOUSE_BUTTON_RIGHT)
    {
        mouseColor = MOUSE_COLOR_RIGHT;
        return;
    }

    if (button == MOUSE_BUTTON_MIDDLE)
    {
        mouseColor = MOUSE_COLOR_MIDDLE;
        return;
    }
}

void main(void) {
    if (framebuffer_request.response == null) {
        hcf2();
    }
    // Fetch the first framebuffer.
    framebuffer = framebuffer_request.response->framebuffers[0];

    init_heap(3 MiB);

    ft_ctx = flanterm_fb_simple_init(
        framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch
    );
    isBufferReady = yes;

    if(framebuffer_request.response->framebuffer_count > 1){
        info("Multiple framebuffers detected! using the first one.", __FILE__);
    }

    // init_hardware_abstraction_layer();

    // if(logoBoot){
    //     print(boot_logo);
    //     print(" Welcome to FrostWing Operating System! (https://github.com/Frost-Wing)\n");
    //     print(red_color "=Terminal Window");
    //     for(int i = 0; i < (ft_ctx->cols); i++){
    //         print("=");
    //     }
    //     print(reset_color);
    //     isBufferReady = no;
    //     ft_ctx = flanterm_fb_simple_init(
    //         (int64)framebuffer->address + (int64)(framebuffer->pitch * (framebuffer->height / 2)), framebuffer->width, (framebuffer->height / 2), framebuffer->pitch
    //     );
    //     ft_ctx->set_cursor_pos(ft_ctx, 0, framebuffer->height - (framebuffer->height / 2));
    //     isBufferReady = yes;
    // }

    terminal_rows = ft_ctx->rows;
    terminal_columns = ft_ctx->cols;
    fb_width = framebuffer->width;
    fb_height = framebuffer->height;

    acpi_init();
    if(virtualized){ // The code inside this will not work on a real machine.
        probe_serial();
    }

    RTL8139 = (struct rtl8139*)malloc(sizeof(struct rtl8139));

    struct memory_context memory;
    memory.total = 0;
    memory.usable = 0;
    memory.reserved = 0;
    memory.acpi_reclaimable = 0;
    memory.acpi_nvs = 0;
    memory.bad = 0;
    memory.bootloader_reclaimable = 0;
    memory.kernel_modules = 0;
    memory.framebuffer = 0;
    memory.unknown = 0;
    for (size_t i = 0; i < memory_map_request.response->entry_count; ++i) {
        int length = memory_map_request.response->entries[i]->length;
        memory.total += length;
        string type = "";
        switch (memory_map_request.response->entries[i]->type)
        {
            case 0:
                memory.usable += length;
                type = "Usable";
                break;
            case 1:
                memory.reserved += length;
                type = "Reserved";
                break;
            case 2:
                memory.acpi_reclaimable += length;
                type = "ACPI Reclaimable";
                break;
            case 3:
                memory.acpi_nvs += length;
                type = "ACPI NVS";
                break;
            case 4:
                memory.bad += length;
                type = "Faulty";
                break;
            case 5:
                memory.bootloader_reclaimable += length;
                type = "Bootloader Reclaimable";
                break;
            case 6:
                memory.kernel_modules += length;
                type = "Kernel & Modules";
                break;
            case 7:
                memory.framebuffer += length;
                type = "Framebuffer";
                break;
            default:
                memory.unknown += length;
                type = "Unknown";
        }
        printf("Base: 0x%x, Length: 0x%x, Type: %s", memory_map_request.response->entries[i]->base, length, type);
    }

    probe_pci();

    print(public_key);
    print("\n");
    
    warn("Kernel might hang here once in a while, reboot to fix the issue.", __FILE__);

    sleep(1);

    if(graphics_base_Address != null){
        isBufferReady = no;
        ft_ctx = flanterm_fb_simple_init(
            graphics_base_Address,
            framebuffer->width,
            framebuffer->height,
            framebuffer->pitch
        );
        isBufferReady = yes;
        info("Welcome to FrostWing Operating System!", "(https://github.com/Frost-Wing)");
        done("Displaying using graphics card! (Goodbye framebuffer)", __FILE__);
        print("Graphics card used is " green_color);
        print(using_graphics_card);
        print(reset_color "\n");
    }else{
        warn("Still using framebuffer, graphics card base address is null.", __FILE__);
    }

    printf("Display Resolution: %dx%d (%d) pixels. Pitch: %d", framebuffer->width, framebuffer->height, framebuffer->width*framebuffer->height, framebuffer->pitch);

    info("Memory Values begin! ===", __FILE__);
    printf("Usable                 : %d KiB", memory.usable / 1024);
    printf("Reserved               : %d KiB", memory.reserved / 1024);
    printf("ACPI Reclaimable       : %d KiB", memory.acpi_reclaimable / 1024);
    printf("ACPI NVS               : %d KiB", memory.acpi_nvs / 1024);
    printf("Bad                    : %d KiB", memory.bad / 1024);
    printf("Bootloader Reclaimable : %d KiB", memory.bootloader_reclaimable / 1024);
    printf("Kernel Modules         : %d KiB", memory.kernel_modules / 1024);
    printf("Framebuffer            : %d KiB", memory.framebuffer / 1024);
    printf("Unknown                : %d KiB", memory.unknown / 1024);   print(yellow_color);
    printf("Grand Total            : %d MiB", ((memory.total / 1024)/1024)-3); // There is an error of 3MB always for some reason
    info(reset_color "Memory values end! =====", __FILE__);

    if(memory.bad != 0){
        warn("Bad blocks of memory found, it is recommended to replace your RAM.", __FILE__);
    }

    if(((memory.total / 1024)/1024) - 3 <= 75){
        warn("INSUFFICIENT MEMORY TO PROCEED WITH RE-INITIALIZATION HEAP!", __FILE__);
    } else{
        // Re-initializing heap with vast memory.
        init_heap(memory.usable / 2);
    }

    printf("Total CPU(s): %d", smp_request.response->cpu_count);
    for(int i=0;i<smp_request.response->cpu_count;i++){
        printf("Processor  ID [%d] : 0x%x", i+1, smp_request.response->cpus[i]->processor_id);
        printf("Local APIC ID [%d] : 0x%x", i+1, smp_request.response->cpus[i]->lapic_id);
#if defined (__x86_64__)
        if (smp_request.response->cpus[i]->lapic_id !=  smp_request.response->bsp_lapic_id) {
#elif defined (__aarch64__)
        if (smp_request.response->cpus[i]->mpidr != smp_request.response->bsp_mpidr) {
#elif defined (__riscv)
        if (smp_request.response->cpus[i]->hartid != smp_request.response->bsp_hartid) {
#endif
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

    frost_compilation_information();

    // initialize_page_bitmap();

    initIdt();
    
    init_hashing();

    // gdt_init();

    // ~ Negative numbers test
    int test_var = 50;
    // test_var = -test_var;
    // if(test_var == -50){
    //     done("Works!", __FILE__);
    // }

    // printf("%d", test_var);
    printf("%X", test_var);

    // "OpenGL" context creation/destroying and triangle/line drawing test code (actual opengl-like implementations coming soon(tm))
    // glCreateContext();
    // glDrawLine((uvec2){0, 0}, (uvec2){10, 30}, 0xffbaddad);
    // glDrawTriangle((uvec2){10, 10}, (uvec2){100, 100}, (uvec2){100, 10}, 0xffdadbad, false);
    // glDrawTriangle((uvec2){110, 110}, (uvec2){200, 200}, (uvec2){200, 110}, 0xffdadbad, true);
    // glDrawRect((uvec2){110, 110}, (uvec2){200, 200}, 0xffdadbad);
    // glDestroyContext(null);

    glCreateContext();
    glCreateContextCustom(framebuffer->address, framebuffer->width, framebuffer->height);
    init_ps2_mouse();
    SetMouseMovementHandler(mouseMovementHandler);
    SetMouseButtonHandler(mouseButtonHandler);

    // glDestroyContext(null);

    // Below code is for triggering Page Fault
    // volatile uint32_t *ptr = (volatile uint32_t *)0xFFFFFFFFFFFFF000;
    // uint32_t value = *ptr;

    // int val0 = 10;
    // int val1 = 20;
    // list my_list;
    // list_init(&my_list);
    // printf("list is empty: %d", list_empty(&my_list));
    // list_push_back(&my_list, &val0);
    // printf("list size: %d", my_list.size);
    // list_push_back(&my_list, &val1);
    // printf("list size: %d", my_list.size);
    // void* resultVal = list_pop_back(&my_list);
    // printf("list size: %d", my_list.size);
    // printf("list last pop-ed value: %d", *(int*)resultVal);
    // list_clear(&my_list);
    // printf("list size: %d", my_list.size);

    extract_tarball(module_request.response->modules[0]->address);

    // allocate_memory_at_address((int64)malloc(0x32), 0x32);

    ssfn_src = (ssfn_font_t*)module_request.response->modules[1]->address;      /* the bitmap font to use */

    ssfn_dst.ptr = framebuffer->address;
    ssfn_dst.w = framebuffer->width;
    ssfn_dst.h = framebuffer->height;
    ssfn_dst.p = framebuffer->pitch;                          /* bytes per line */
    ssfn_dst.x = ssfn_dst.y = 0;
    ssfn_dst.fg = 0xFFFFFF;
    ssfn_dst.bg = 0x000000;

    ft_ctx->cursor_enabled = no;

    // execute_elf(module_request.response->modules[2]->address);

    print("press F10 for (ACPI) Shutdown.\n");
    print("press F9 for (ACPI/Hard) Reboot/Reset.\n");

    done("No process pending.", __FILE__);

    // print("\x1b[2J"); // Clears screen
    // print("\x1b[H");  // Resets Cursor to 0, 0


    // glCreateContext();
    // glCreateContextCustom(front_buffer, framebuffer->width, framebuffer->height);
    // glClearColor(0, 0, 0, 0xff);
    // glClear(GL_COLOR_BUFFER_BIT);
    // glDrawTriangle((uvec2){10, 10}, (uvec2){100, 100}, (uvec2){100, 10}, 0xffdadbad, false);
    // glDestroyContext(null);
    while(1){
        // decode_targa_image(module_request.response->modules[2]->address, (uvec2){0, 0}, framebuffer->width, framebuffer->height);

        // ssfn_dst.x = ssfn_dst.y = 0;
        // ssfn_print("Hello!");

        // // ivec2 lastMousePos = GetLastMousePosition();
        // ivec2 mousePos = GetMousePosition();
        // decode_targa_image(module_request.response->modules[3]->address, (uvec2){mousePos.x, mousePos.y}, framebuffer->width, framebuffer->height);

        // pit_sleep((int32)16.6666); // 60 Hz approx.
    }
}

void ssfn_print(cstring msg){
    while(*msg){
        ssfn_putc(*msg);
        msg++;
    }
}

/**
 * @brief The basic print function.
 * 
 * @param msg The message to be printed
 */
void print(cstring msg){
    if(!isBufferReady) return;
    if(msg == null){
        flanterm_write(ft_ctx, "null", 4);
        return;
    }
    flanterm_write(ft_ctx, msg, strlen_(msg));
}

/**
 * @brief The basic put char function.
 * 
 * @param c The char to be printed
 */
void putc(char c){
    if(!isBufferReady) return;
    ft_ctx->raw_putchar(ft_ctx, c);
}

/**
 * @brief ACPI Shutdown code.
 * 
 */
void shutdown(){
    cleanup_heap();

    acpi_shutdown_hack(hhdm_request.response->offset, acpi_find_sdt, inb, inw, outb, outw);
}