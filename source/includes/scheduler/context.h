/**
 * @file context.h
 * @author Pradosh
 * @brief x86_64 CPU context definitions for FrostWing.
 *
 * This file defines every CPU state required to suspend and resume
 * kernel or userspace tasks.
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * --------------------------------------------------------------------------
 * General Purpose Registers
 * --------------------------------------------------------------------------
 *
 * Saved during a context switch.
 *
 * Matches x86-64 register names exactly.
 */

typedef struct cpu_context
{
    uint64_t r15;   // +0
    uint64_t r14;   // +8
    uint64_t r13;   // +16
    uint64_t r12;   // +24
    uint64_t rbx;   // +32
    uint64_t rbp;   // +40

    uint64_t rsp;   // +48
    uint64_t rflags;// +56
    uint64_t rip;   // +64
} cpu_context_t;

/*
 * --------------------------------------------------------------------------
 * Trap Frame
 * --------------------------------------------------------------------------
 *
 * Layout pushed by interrupt/syscall entry before calling C.
 *
 * switch.S and scheduler.c should use this structure directly.
 */

typedef struct trap_frame {

    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;

    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;

    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;

    uint64_t rsi;
    uint64_t rdi;

    uint64_t vector;
    uint64_t error;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;

} trap_frame_t;

/*
 * --------------------------------------------------------------------------
 * Complete process execution context.
 * --------------------------------------------------------------------------
 */

typedef struct process_context {

    cpu_context_t cpu;

    /*
     * Physical address of page table (CR3).
     */
    uint64_t cr3;

    /*
     * Kernel stack top.
     */
    uint64_t kernel_stack;

    /*
     * Userspace stack top.
     */
    uint64_t user_stack;

    /*
     * Thread-local storage base.
     * (arch_prctl ARCH_SET_FS)
     */
    uint64_t fs_base;

    /*
     * GS base if used later.
     */
    uint64_t gs_base;

} process_context_t;

/*
 * --------------------------------------------------------------------------
 * Context management.
 * --------------------------------------------------------------------------
 */

void context_init(process_context_t *ctx);

void context_copy(
    process_context_t *dst,
    const process_context_t *src);

void context_from_trapframe(
    process_context_t *ctx,
    const trap_frame_t *frame);

void context_to_trapframe(
    trap_frame_t *frame,
    const process_context_t *ctx);

/*
 * Returns true if currently executing in userspace.
 */
bool context_is_user(
    const process_context_t *ctx);

#endif