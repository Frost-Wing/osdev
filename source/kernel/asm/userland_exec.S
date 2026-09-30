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

    .section .note.GNU-stack,"",@progbits
