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
#include "fb.h"
#include "fdlfcn.h"
#include <kernel.h>
#include <stdint.h>

int terminal_rows = 0;
int terminal_columns = 0;

int64 fb_width = 0;
int64 fb_height = 0;

int64* wm_addr;

int64* font_address = null;

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

struct flanterm_context *ft_ctx = null;
struct limine_framebuffer *framebuffer = null;

bool isBufferReady = no;

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

struct memory_context memory;

void main(void) {
    if (framebuffer_request.response == null) {
        hcf2();
    }
    // Fetch the first framebuffer.
    framebuffer = framebuffer_request.response->framebuffers[0];

    ft_ctx = flanterm_fb_simple_init(
        framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch
    );
    isBufferReady = yes;

    if(framebuffer_request.response->framebuffer_count > 1){
        info("Multiple framebuffers detected! using the first one.", __FILE__);
    }

    terminal_rows = ft_ctx->rows;
    terminal_columns = ft_ctx->cols;
    fb_width = framebuffer->width;
    fb_height = framebuffer->height;

    if(virtualized){ // The code inside this will not work on a real machine.
        probe_serial();
    }
    
    mm_init((uintptr_t)0x1000000);
    acpi_init();

    printf("KERNEL END -> 0x%X", (uintptr_t)kend);
    
    // * OS FAILSAFE -> IF MALLOC FAILS.. IT FAILS HERE RATHER THAN MAIN CODE SO THIS SHOULD NOT BE REMOVED.
    uint8_t* a = (uint8_t*)kmalloc(16);
    uint8_t* b = (uint8_t*)kmalloc(32);
    
    *a = 0x42;
    b[0] = 0x99;
    
    mm_print_out();

    setup_gdt();
    
    tss_init();
    tss_load();
    
    RTL8139 = (struct rtl8139*) kmalloc(sizeof(struct rtl8139));

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

    if(virtualized){
        if(graphics_base_Address != null){
            isBufferReady = no;
            ft_ctx = flanterm_fb_simple_init(
                (uint32_t*)graphics_base_Address,
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
    }

    printf("Display Resolution: %dx%d (%d) pixels. Pitch: %d", framebuffer->width, framebuffer->height, framebuffer->width*framebuffer->height, framebuffer->pitch);

    info("Memory Values begin! ===", __FILE__);
    display_memory_formatted(memory);
    info(reset_color "Memory values end! =====", __FILE__);

    if(memory.bad != 0){
        warn("Bad blocks of memory found, it is recommended to replace your RAM.", __FILE__);
    }

    printf("Total CPU(s): %d", smp_request.response->cpu_count);
    for(int i=0;i<smp_request.response->cpu_count;i++){
        printf("Processor  ID [%d] : 0x%x", i+1, smp_request.response->cpus[i]->processor_id);
        printf("Local APIC ID [%d] : 0x%x", i+1, smp_request.response->cpus[i]->lapic_id);

        if (smp_request.response->cpus[i]->lapic_id !=  smp_request.response->bsp_lapic_id) {
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

    initIdt();
    
    init_hashing();

    initialize_page_bitmap();
    
    // "OpenGL" context creation/destroying and triangle/line drawing test code (actual opengl-like implementations coming soon(tm))
    // glCreateContext();
    // glDrawLine((uvec2){0, 0}, (uvec2){10, 30}, 0xffbaddad);
    // glDrawTriangle((uvec2){10, 10}, (uvec2){100, 100}, (uvec2){100, 10}, 0xffdadbad, false);
    // glDrawTriangle((uvec2){110, 110}, (uvec2){200, 200}, (uvec2){200, 110}, 0xffdadbad, true);
    // glDrawRect((uvec2){110, 110}, (uvec2){200, 200}, 0xffdadbad);
    // glDestroyContext(null);

    // glCreateContext();
    // glCreateContextCustom(framebuffer->address, framebuffer->width, framebuffer->height);
    // init_ps2_mouse();
    // SetMouseMovementHandler(mouseMovementHandler);
    // SetMouseButtonHandler(mouseButtonHandler);

    mm_print_out();

    create_user_str("root", "prad");

    enable_fpu();

    // enter_userland();

    info("Welcome to FrostWing Operating System!", "(https://github.com/Frost-Wing)");

    // glCreateContext();
    // glCreateContextCustom(framebuffer->address, framebuffer->width, framebuffer->height);
    // glClearColor(0, 0, 0, 0xff);
    // glClear(GL_COLOR_BUFFER_BIT);
    // display_png(module_request.response->modules[1]->address, module_request.response->modules[1]->size);
    // glDrawTriangle((uvec2){10, 10}, (uvec2){100, 100}, (uvec2){100, 10}, 0xffdadbad, false);
    // glDestroyContext(null);

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

    // extract_tarball(module_request.response->modules[0]->address);

    // allocate_memory_at_address((int64)kmalloc(0x32), 0x32);

    // font_address = module_request.response->modules[1]->address;

    // kernel_data* data = (kernel_data*) kmalloc(sizeof(kernel_data));
    // data->fb_addr = framebuffer->address;
    // data->width = framebuffer->width;
    // data->height = framebuffer->height;
    // data->pitch = framebuffer->pitch;
    // data->print = print;
    // execute_fwde(module_request.response->modules[2]->address, data);

    // kfree(data);

    // print("\x1b[2J"); // Clears screen
    // print("\x1b[H");  // Resets Cursor to 0, 0

    // int* test1 = kmalloc(sizeof(int));
    // int* test2 = kmalloc(sizeof(int));
    // int* test3 = kmalloc(sizeof(int));
    // int* test4 = kmalloc(sizeof(int));

    wm_addr = module_request.response->modules[0]->address;

    int failed_attempts = 0;

    // void* binaddress = module_request.response->modules[1]->address;
    // printf("binary addr: 0x%x", binaddress);
    // int (*execute_binary)(void) = binaddress;
    // int status_code = execute_binary();
    // printf("return code: 0x%x", status_code);


    extern char* login_request();

    while(1){
        // decode_targa_image(module_request.response->modules[2]->address, (uvec2){0, 0}, framebuffer->width, framebuffer->height);


        // // ivec2 lastMousePos = GetLastMousePosition();
        // ivec2 mousePos = GetMousePosition();
        // decode_targa_image(module_request.response->modules[3]->address, (uvec2){mousePos.x, mousePos.y}, framebuffer->width, framebuffer->height);

        if (failed_attempts >= 5){
            error("You tried 5 diffrent wrong attempts. You've been locked out.", __FILE__);
            hcf();
        }

        char* username = login_request();
        
        if(username != NULL){
            int argc = 1;

            int isSudo = 0;
            if(strcmp(username, "root") == 0)
                isSudo = 1;
            
            char* dummy_argv[] = {username, (char*)isSudo};
            shell_main(argc, dummy_argv);
        } else {
            error("Invalid credentials.", __FILE__);
            failed_attempts++;
        }
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
extern void __putc(char c);
void putc(char c){
    if(!isBufferReady)
        return;


    if (c == '\b')
    {
        __putc('\b');
        __putc(' ');
    }

    __putc(c);
}

/**
 * @brief ACPI Shutdown code.
 * 
 */
void shutdown(){
    acpi_shutdown_hack(hhdm_request.response->offset, acpi_find_sdt, inb, inw, outb, outw);
}