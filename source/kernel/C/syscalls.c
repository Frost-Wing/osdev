/**
 * @file syscalls.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-01-05
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */
#include <commands/login.h>
#include <syscalls.h>
#include <limine.h> // for framebuffer
#include <keyboard.h>
#include <graphics.h>

extern struct limine_framebuffer *framebuffer;
extern int64* font_address;

extern int execute_chain(const char* line);

static int64 sys_read(uint64_t fd, char* buf, uint64_t count) {
    if (buf == NULL || count == 0)
        return 0;

    if (fd != 0)
        return -1;

    for (uint64_t i = 0; i < count; ++i) {
        char c = getc();
        buf[i] = c;
        if (c == '\n' || c == '\r')
            return (int64)(i + 1);
    }

    return (int64)count;
}

static int64 sys_write(uint64_t fd, const char* buf, uint64_t count) {
    if (buf == NULL || count == 0)
        return 0;

    if (fd > 2)
        return -1;

    for (uint64_t i = 0; i < count; ++i)
        putc(buf[i]);

    return (int64)count;
}

void invoke_syscall(int64 num) {
    asm volatile (
        "movq %0, %%rax\n\t"
        "int $0x80\n\t"
        :
        : "g" ((int64)num)
        : "rax"
    );
}

void syscalls_handler(InterruptFrame* frame){
    switch (frame->rax)
    {
        case 0:
            frame->rax = sys_read(frame->rdi, (char*)frame->rsi, frame->rdx);
            break;
        case 1:
            frame->rax = sys_write(frame->rdi, (const char*)frame->rsi, frame->rdx);
            break;
        case 59:
            frame->rax = execute_chain((const char*)frame->rdi);
            break;
        case 60:
            frame->rax = 0;
            break;
        case 0x10:
            frame->rax = getc();
            break;
        case 0x11:
            frame->rax = kgetc_nonblock();
            break;
        case 0x12:
            printfnoln("%c", (char)frame->rdi);
            break;
        case 0x55:
            frame->rax = login_request((char*)frame->rdi, frame->rsi);
            break;
        default:
            error(syscalls_prefix "Unknown", __FILE__);
            printf("RAX value -> 0x%x", frame->rax);
            hcf2();
            break;
    }

    outb(0x20, 0x20); // End PIC Master
    outb(0xA0, 0x20); // End PIC Slave
}
