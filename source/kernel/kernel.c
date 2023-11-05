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
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

static volatile struct limine_hhdm_request hhdm_req = {
    LIMINE_HHDM_REQUEST, 0, NULL
};

struct flanterm_context *ft_ctx = NULL;
struct limine_framebuffer *framebuffer = NULL;

uint64_t* back_buffer;
uint64_t* front_buffer;

void render(int width, int height){
    for(int i = 0; i < width * height; i++){
        front_buffer[i] = back_buffer[i];
    }
}

/**
 * @brief The main kernel function
 * renaming main() to something else, make sure to change the linker script accordingly.
 */
void main(void) {
    if (framebuffer_request.response == NULL) {
        asm("hlt");
    }
    // Fetch the first framebuffer.
    framebuffer = framebuffer_request.response->framebuffers[0];

    uint64_t temp_buffer[framebuffer->width * framebuffer->height];
    front_buffer = (uint64_t*)framebuffer->address;
    back_buffer = front_buffer;
    back_buffer = (uint64_t*)temp_buffer;
    ft_ctx = flanterm_fb_simple_init(
        front_buffer, framebuffer->width, framebuffer->height, framebuffer->pitch
    );
    

    terminal_rows = ft_ctx->rows;
    terminal_columns = ft_ctx->cols;
    if(framebuffer_request.response->framebuffer_count < 1){
        warn("Multiple framebuffers detected! Using Framebuffer[0] (You probably have 2 monitors)", __FILE__);
    }
    acpi_init();
    if(virtualized){ // The code inside this will not work on a real machine.
        gdt_init();
        probe_serial();
    }
    load_typescript();
    probe_pci();

    get_cpu_name();
    print_cpu();
    L1_cache_size();
    L2_cache_size();
    L3_cache_size();
    init_rtc();
    display_time();

    enable_fpu();

    check_sse();
    load_complete_sse();

    // "OpenGL" context creation/destroying and triangle/line drawing test code (actual opengl-like implementations coming soon(tm))
    // glCreateContext();
    // glDrawLine((uvec2){0, 0}, (uvec2){10, 30}, 0xffbaddad);
    // glDrawTriangle((uvec2){10, 10}, (uvec2){100, 100}, (uvec2){100, 10}, 0xffdadbad, false);
    // glDrawTriangle((uvec2){110, 110}, (uvec2){200, 200}, (uvec2){200, 110}, 0xffdadbad, true);
    // glDestroyContext(NULL);

    done("No process pending, press \'F10\' to call ACPI Shutdown.", __FILE__);

    // glCreateContext();
    // glCreateContextCustom(front_buffer, framebuffer->width, framebuffer->height);
    // glClearColor(0, 0, 0, 0xff);
    // glClear(GL_COLOR_BUFFER_BIT);
    // glDrawTriangle((uvec2){10, 10}, (uvec2){100, 100}, (uvec2){100, 10}, 0xffdadbad, false);
    // glDestroyContext(NULL);

    // render(framebuffer->width, framebuffer->height);

    while(1){
        set_keyboard_leds(2); // Num lock
        sleep(1);
        set_keyboard_leds(4); // Caps lock
        sleep(1);
        set_keyboard_leds(1); // Scroll lock
        sleep(1);
    }
}

/**
 * @brief The basic print function.
 * 
 * @param msg The message to be printed
 */
void print(const char* msg){
    flanterm_write(ft_ctx, msg, strlen_(msg));
}

/**
 * @brief ACPI Shutdown code.
 * 
 */
void shutdown(){
    acpi_shutdown_hack(hhdm_req.response->offset, acpi_find_sdt, inb, inw, outb, outw);
}

/**
 * @brief control for keyboard LEDs just for an easter egg lol
 * 
 */
void set_keyboard_leds(int LEDs) {
    outb(0x64, 0xED);
    outb(0x60, LEDs);
}
