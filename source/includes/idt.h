/**
 * @file idt.h
 * @author Pradosh (pradoshgame@gmail.com) & GAMINGNOOB
 * @brief 
 * @version 0.1
 * @date 2025-02-03
 * 
 * @copyright Copyright Pradosh (c) 2025
 * 
 */


#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

#define IA32_EFER   0xC0000080
#define IA32_STAR   0xC0000081
#define IA32_LSTAR  0xC0000082
#define IA32_FMASK  0xC0000084

#define EFER_SCE (1 << 0)

typedef struct IDTEntry
{
        uint16_t offset_1;        // offset bits 0..15
        uint16_t selector;        // a code segment selector in GDT or LDT
        uint8_t  ist;             // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
        uint8_t  type_attributes; // gate type, dpl, and p fields
        uint16_t offset_2;        // offset bits 16..31
        uint32_t offset_3;        // offset bits 32..63
        uint32_t zero;            // reserved
}__attribute__((packed)) IDTEntry;

typedef struct IDTPointer 
{
        uint16_t size;
        uint64_t offset;
}__attribute__((packed)) IDTPointer;

/**
 * @brief Located inside /interrupts/syscall-x86_64.asm
 * 
 */
extern void syscall_entry(void);

/**
 * @brief Tells the CPU to use the syscall instruction.
 * 
 */
void init_syscall();

void initIdt();
void setIdtEntry(IDTEntry *target, uint64_t offset, uint16_t selector, uint8_t ist, uint8_t type_attributes);

#endif