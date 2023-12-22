#include <idt.h>
#include <isr.h>
#include <keyboard.h>
#include <ps2-mouse.h>

extern void* isr_stub_table[];
extern void* irq_stub_table[];

IDTEntry idt_entries[256];

IDTPointer idt_ptr = (IDTPointer) 
{
    (uint16_t)sizeof(idt_entries) - 1,
    (uintptr_t)&idt_entries[0]
};

void setIdtEntry(IDTEntry *target, uint64_t offset, uint16_t selector, uint8_t ist, uint8_t type_attributes) 
{
    target->offset_1 = offset & 0xFFFF;
    target->selector = selector;
    target->ist = ist;
    target->type_attributes = type_attributes;
    target->offset_2 = (offset >> 16) & 0xFFFF;
    target->offset_3 = (offset >> 32) & 0xFFFFFFFF;
    target->zero = 0;
}

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

/**
 * @brief Small utility function for remapping the programmable interrupt controller
 * @author GAMINGNOOBdev
 */
void remap_pic() {
    uint8_t read_master, read_slave;

    read_master = inb(0x21);
    read_slave = inb(0xa1);

    outb(0x20, ICW1_INIT | ICW1_ICW4);
    outb(0xa0, ICW1_INIT | ICW1_ICW4);

    outb(0x21, 0x20);
    outb(0xa1, 0x28);

    outb(0x21, 0x04);
    outb(0xa1, 0x02);

    outb(0x21, ICW4_8086);
    outb(0xa1, ICW4_8086);

    outb(0x21, read_master);
    outb(0xa1, read_slave);
}

void initIdt() 
{
    info("Started initialization!", __FILE__);

    // interrupt number of keyboard is 0x21, for pit it's 0x20 and for mouse it's 0x2C
    registerInterruptHandler(0x21, process_keyboard);
    // registerInterruptHandler(0x2C, process_mouse);

    for (uint8_t i = 0; i < 32; i++) 
    {
        setIdtEntry(&idt_entries[i], (uint64_t)isr_stub_table[i], 0x28, 0, 0x8E);
    }

    for (uint8_t i = 32; i < 255; i++) 
    {
        setIdtEntry(&idt_entries[i], (uint64_t)irq_stub_table[i-32], 0x28, 0, 0x8E);
    }

    remap_pic();

    outb(0x21, 0xfd); // 0xfd for keyboard only and for mouse + keyboard 0xf8 (if sb 16 support then 0x??)
    outb(0xa1, 0xff); // 0xff for keyboard only and for mouse + keyboard 0xef

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
    __asm__ volatile("sti");

    done("Successfully initialized!", __FILE__);
}