/**
 * @file isr.h
 * @author Pradosh (pradoshgame@gmail.com) & GAMINGNOOB
 * @brief Interrupts handler header.
 * @version 0.1
 * @date 2025-02-03
 * 
 * @copyright Copyright Pradosh (c) 2025
 * 
 */
#ifndef ISR_H
#define ISR_H

#include <stdint.h>
#include <stddef.h>
#include <graphics.h>

typedef struct InterruptFrame
{
    uint64_t r11, r10, r9, r8;
    uint64_t rsi, rdi, rdx, rcx, rax;
    uint64_t int_no, err_code;
    uint64_t rsp, rflags, cs, rip;
}__attribute__((packed)) InterruptFrame;

/**
 * @brief A function pointer for interrupt handlers
 */
typedef void(*irq_handler)(InterruptFrame*);

void exceptionHandler(InterruptFrame* frame);
void irqHandler(InterruptFrame* frame); 

/**
 * @brief Registers a handler for an interrupt
 * 
 * @param irq The interrupt number of the handler
 * @param handler Pointer to the handler function
 */
void registerInterruptHandler(uint8_t irq, irq_handler handler);

void rtl8139_handler(InterruptFrame* frame);

#endif