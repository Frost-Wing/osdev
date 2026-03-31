/**
 * @file isr.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Interrupts handler code.
 * @version 0.1
 * @date 2025-02-03
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <isr.h>
#include <keyboard.h>
#include <memory.h>
#include <drivers/rtl8139.h>
#include <syscalls.h>
#include <debugger.h>

irq_handler interrupt_handlers[256];

static int skip_endbr64_if_present(InterruptFrame* frame) {
    if (!frame)
        return 0;

    if ((frame->cs & 0x3) != 0x3)
        return 0;

    uint8_t* ip = (uint8_t*)frame->rip;
    if (!ip)
        return 0;

    if (ip[0] == 0xF3 && ip[1] == 0x0F && ip[2] == 0x1E && ip[3] == 0xFA) {
        debug_printf("isr: skipping ENDBR64 at rip=0x%X\n", frame->rip);
        frame->rip += 4;
        return 1;
    }

    return 0;
}

static void log_page_fault_details(InterruptFrame* frame) {
    if (!frame || frame->int_no != 14)
        return;

    uint64_t err = frame->err_code;
    eprintf("pagefault: rip=0x%X addr=0x%X err=0x%X present=%d write=%d user=%d reserved=%d fetch=%d",
            frame->rip,
            getCR2(),
            err,
            (int)(err & 0x1),
            (int)((err >> 1) & 0x1),
            (int)((err >> 2) & 0x1),
            (int)((err >> 3) & 0x1),
            (int)((err >> 4) & 0x1));
}

static int emulate_syscall_instruction_if_present(InterruptFrame* frame) {
    if (!frame)
        return 0;

    if ((frame->cs & 0x3) != 0x3)
        return 0;

    uint8_t* ip = (uint8_t*)frame->rip;
    if (!ip)
        return 0;

    if (ip[0] == 0x0F && ip[1] == 0x05) {
        debug_printf("isr: emulating SYSCALL as int 0x80 at rip=%u nr=%u\n", frame->rip, frame->rax);
        syscalls_handler(frame);
        frame->rip += 2;
        return 1;
    }

    return 0;
}

void exceptionHandler(InterruptFrame* frame) {
    enable_logging = false; // disables logger as fast as it can to get the last instance of panic.
    log_page_fault_details(frame);
    
	switch (frame->int_no) {
        case 0:
            meltdown_screen("Arithmetical operation with division of zero detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
			break;
        case 5:
            meltdown_screen("Bound Range exceeded!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
			break;
        case 6:
            meltdown_screen("Invalid opcode detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
			break;
        case 8:
            meltdown_screen("Double fault detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
			break;
        case 10:
            meltdown_screen("Invalid TSS detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
			break;
        case 13:
            meltdown_screen("General protection violation detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
			break;
		case 14:
            meltdown_screen("Page protection violation detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
			break;
        case 16:
            meltdown_screen("x87 Floating-Point violation detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
			break;
        case 19:
            meltdown_screen("SIMD Floating-Point violation detected! (SSE Related issue)", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
			break;
        default:
            meltdown_screen("Unknown exception detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no, frame);
	}
    
    hcf2();
}

void registerInterruptHandler(uint8_t interrupt, irq_handler handler)
{
    interrupt_handlers[interrupt] = handler;
}

void irqHandler(InterruptFrame* frame)
{
    irq_handler handler = (irq_handler)interrupt_handlers[frame->int_no];
    if (handler != NULL)
        handler(frame);
}

void rtl8139_handler(InterruptFrame* frame) {
	uint16_t status = inw(RTL8139->io_base + 0x3e);
	outw(RTL8139->io_base + 0x3E, 0x05);
	if(status & TOK) {
		info("Successfully sent a packet!", __FILE__);
	}
	if (status & ROK) {
		info("Successfully recieved a packet!", __FILE__);
	}
}
