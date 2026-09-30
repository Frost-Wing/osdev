/*
 * ABI boundary for a userland run.  This function deliberately has no C
 * prologue: the values saved below are the callee-saved registers owned by
 * userland_exec's caller, not compiler temporaries belonging to a wrapper.
 */
    .text

    .globl userland_exec
    .type userland_exec, @function
userland_exec:
    /* Entry RSP is 8 mod 16.  Reserve caller state plus 8 bytes for alignment. */
    subq $72, %rsp

    /* userland_caller_state_t at RSP; RDI still holds ctx. */
    movq %rbx, 16(%rsp)
    movq %rbp, 24(%rsp)
    movq %r12, 32(%rsp)
    movq %r13, 40(%rsp)
    movq %r14, 48(%rsp)
    movq %r15, 56(%rsp)
    leaq .Luserland_exec_resume(%rip), %rax
    movq %rax, 0(%rsp)
    movq %rsp, 8(%rsp)

    movq %rsp, %rsi
    call userland_exec_impl

.Luserland_exec_resume:
    /* EAX is either userland_exec_impl's failure code or the exit code. */
    addq $72, %rsp
    ret
    .size userland_exec, .-userland_exec

/* regs: rbx, rbp, r12, r13, r14, r15, eax, rsp, rip. */
    .globl userland_jump_resume
    .type userland_jump_resume, @function
userland_jump_resume:
    movq 0(%rdi), %rbx
    movq 16(%rdi), %r12
    movq 24(%rdi), %r13
    movq 32(%rdi), %r14
    movq 40(%rdi), %r15
    movl 48(%rdi), %eax
    movq 64(%rdi), %rdx
    movq 8(%rdi), %rbp
    movq 56(%rdi), %rsp
    jmp *%rdx
    .size userland_jump_resume, .-userland_jump_resume

    .globl userland_fork
    .type userland_fork, @function
userland_fork:
    subq $72, %rsp
    movq %rbx, 16(%rsp)
    movq %rbp, 24(%rsp)
    movq %r12, 32(%rsp)
    movq %r13, 40(%rsp)
    movq %r14, 48(%rsp)
    movq %r15, 56(%rsp)
    leaq .Luserland_fork_resume(%rip), %rax
    movq %rax, 0(%rsp)
    movq %rsp, 8(%rsp)
    movq %rsp, %rsi
    call userland_fork_impl
.Luserland_fork_resume:
    addq $72, %rsp
    ret
    .size userland_fork, .-userland_fork

/* rdi = userland_regs_t* */
    .globl userland_iret_regs
    .type userland_iret_regs, @function
userland_iret_regs:
    cli
    pushq $0x23
    pushq 136(%rdi)
    pushq 128(%rdi)
    pushq $0x1B
    pushq 120(%rdi)
    movq 8(%rdi),  %rbx
    movq 16(%rdi), %rcx
    movq 24(%rdi), %rdx
    movq 32(%rdi), %rsi
    movq 48(%rdi), %rbp
    movq 56(%rdi), %r8
    movq 64(%rdi), %r9
    movq 72(%rdi), %r10
    movq 80(%rdi), %r11
    movq 88(%rdi), %r12
    movq 96(%rdi), %r13
    movq 104(%rdi), %r14
    movq 112(%rdi), %r15
    movq 0(%rdi),  %rax
    movq 40(%rdi), %rdi
    iretq
    .size userland_iret_regs, .-userland_iret_regs
    
    .section .note.GNU-stack,"",@progbits
