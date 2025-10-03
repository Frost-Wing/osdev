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
#include <memory2.h>
#include <drivers/rtl8139.h>

irq_handler interrupt_handlers[256];

void exceptionHandler(InterruptFrame* frame) {
	switch (frame->int_no) {
        case 0:
            meltdown_screen("Arithmetical operation with division of zero detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
			break;
        case 5:
            meltdown_screen("Bound Range exceeded!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
			break;
        case 6:
            meltdown_screen("Invalid opcode detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
			break;
        case 8:
            meltdown_screen("Double fault detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
			break;
        case 10:
            meltdown_screen("Invalid TSS detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
			break;
        case 13:
            meltdown_screen("General protection violation detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
			break;
		case 14:
            meltdown_screen("Page protection violation detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
			break;
        case 16:
            meltdown_screen("x87 Floating-Point violation detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
			break;
        case 19:
            meltdown_screen("SIMD Floating-Point violation detected! (SSE Related issue)", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
			break;
        default:
            meltdown_screen("Unknown exception detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
            hcf2();
	}
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
