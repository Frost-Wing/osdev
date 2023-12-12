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

int terminal_rows = 0;
int terminal_columns = 0;

int fb_width = 0;
int fb_height = 0;

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

struct memory_context {
    int64 total;
    int64 usable;
    int64 reserved;
    int64 acpi_reclaimable;
    int64 acpi_nvs;
    int64 bad;
    int64 bootloader_reclaimable;
    int64 kernel_modules;
    int64 framebuffer; // Mostly unneeded because frame buffer struct separately gives it,
    int64 unknown;
};


struct flanterm_context *ft_ctx = null;
struct limine_framebuffer *framebuffer = null;

bool isBufferReady = no;
bool logoBoot = no;

void main(void) {
    if (framebuffer_request.response == null) {
        hcf2();
    }
    // Fetch the first framebuffer.
    framebuffer = framebuffer_request.response->framebuffers[0];

    int64 display_memory_size = (framebuffer->width * framebuffer->height * sizeof(int64));
    init_heap(display_memory_size + (10 MiB));

    ft_ctx = flanterm_fb_simple_init(
        framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch
    );
    isBufferReady = yes;

    if(framebuffer_request.response->framebuffer_count > 1){
        info("Multiple framebuffers detected! using the first one.", __FILE__);
    }

    // init_hardware_abstraction_layer();

    if(logoBoot){
        print(boot_logo);
        print(" Welcome to FrostWing Operating System! (https://github.com/Frost-Wing)\n");
        print(red_color "=Terminal Window");
        for(int i = 0; i < (ft_ctx->cols); i++){
            print("=");
        }
        print(reset_color);
        isBufferReady = no;
        ft_ctx = flanterm_fb_simple_init(
            (int64)framebuffer->address + (int64)(framebuffer->pitch * (framebuffer->height / 2)), framebuffer->width, (framebuffer->height / 2), framebuffer->pitch
        );
        ft_ctx->set_cursor_pos(ft_ctx, 0, framebuffer->height - (framebuffer->height / 2));
        isBufferReady = yes;
    }

    terminal_rows = ft_ctx->rows;
    terminal_columns = ft_ctx->cols;
    fb_width = framebuffer->width;
    fb_height = framebuffer->height;

    if(framebuffer_request.response->framebuffer_count < 1){
        warn("Multiple framebuffers detected! Using Framebuffer[0] (You probably have 2 monitors)", __FILE__);
    }
    acpi_init();
    if(virtualized){ // The code inside this will not work on a real machine.
        probe_serial();
    }
    if(!is_kvm_supported()){
        gdt_init();
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
        printf("Base: %d, Length: %d, Type: %d", memory_map_request.response->entries[i]->base, memory_map_request.response->entries[i]->length, memory_map_request.response->entries[i]->type);
        int length = memory_map_request.response->entries[i]->length;
        memory.total += length;
        switch (memory_map_request.response->entries[i]->type)
        {
            case 0:
                memory.usable += length;
                break;
            case 1:
                memory.reserved += length;
                break;
            case 2:
                memory.acpi_reclaimable += length;
                break;
            case 3:
                memory.acpi_nvs += length;
                break;
            case 4:
                memory.bad += length;
                break;
            case 5:
                memory.bootloader_reclaimable += length;
                break;
            case 6:
                memory.kernel_modules += length;
                break;
            case 7:
                memory.framebuffer += length;
                break;
            default:
                memory.unknown += length;
        }
    }

    probe_pci();

    sleep(2);

    if(graphics_base_Address != null){
        isBufferReady = no;
        ft_ctx = flanterm_fb_simple_init(
            graphics_base_Address, framebuffer->width, framebuffer->height, framebuffer->pitch
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

    printf("Display Resolution: %dx%d pixels. Pitch: %d", framebuffer->width, framebuffer->height, framebuffer->pitch);

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
    printf("Grand Total            : %d MiB", ((memory.total / 1024)/1024)); // There is an error of 3MB always for some reason
    info(reset_color "Memory values end! =====", __FILE__);

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
    
    // "OpenGL" context creation/destroying and triangle/line drawing test code (actual opengl-like implementations coming soon(tm))
    // glCreateContext();
    // glDrawLine((uvec2){0, 0}, (uvec2){10, 30}, 0xffbaddad);
    // glDrawTriangle((uvec2){10, 10}, (uvec2){100, 100}, (uvec2){100, 10}, 0xffdadbad, false);
    // glDrawTriangle((uvec2){110, 110}, (uvec2){200, 200}, (uvec2){200, 110}, 0xffdadbad, true);
    // glDestroyContext(null);

    // glCreateContext();
    // glCreateContextCustom(graphics_base_Address, framebuffer->width, framebuffer->height);
    // init_ps2_mouse();

    // glDestroyContext(null);

    flush_heap();

    done("No process pending.\npress \'F10\' to call ACPI Shutdown.\n", __FILE__);

    // glCreateContext();
    // glCreateContextCustom(front_buffer, framebuffer->width, framebuffer->height);
    // glClearColor(0, 0, 0, 0xff);
    // glClear(GL_COLOR_BUFFER_BIT);
    // glDrawTriangle((uvec2){10, 10}, (uvec2){100, 100}, (uvec2){100, 10}, 0xffdadbad, false);
    // glDestroyContext(null);

    while(1){
        process_keyboard();

        // handle_ps2_mouse(inb(0x60));

        int8 received_buffer[1518];
        int16 received_length;
        if (rtl8139_receive_packet(RTL8139, received_buffer, &received_length)) {
            print("Yep received a packet!\n");
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
void putc(char c){
    if(!isBufferReady) return;
    ft_ctx->raw_putchar(ft_ctx, c);
}

/**
 * @brief ACPI Shutdown code.
 * 
 */
void shutdown(){
    acpi_shutdown_hack(hhdm_request.response->offset, acpi_find_sdt, inb, inw, outb, outw);
}