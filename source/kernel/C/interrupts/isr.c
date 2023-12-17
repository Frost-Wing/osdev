#include <isr.h>

void (*interrupt_handlers[256]) (InterruptFrame* frame);

static inline uint64_t getCR2(void)
{
	uint64_t val;
	__asm__ volatile ( "mov %%cr2, %0" : "=r"(val) );
    return val;
}

void exceptionHandler(InterruptFrame* frame) {
    // printf("0x%x & 0x%x", frame->int_no, frame->err_code);
	switch (frame->int_no) {
        case 0:
            meltdown_screen("Arithmetical operation with division of zero detected!", __FILE__, __LINE__, frame->err_code, getCR2(), frame->int_no);
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
	}
}

void registerInterruptHandler(uint8_t interrupt, void (*handler) (InterruptFrame* frame))
{
    interrupt_handlers[interrupt] = handler;
}

void irqHandler(InterruptFrame* frame)
{
    if (&interrupt_handlers[frame->int_no] != NULL)
    {
        interrupt_handlers[frame->int_no](frame);
    }
}