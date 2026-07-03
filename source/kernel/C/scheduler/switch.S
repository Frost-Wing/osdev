.global context_switch

.text

# void context_switch(cpu_context_t *old, cpu_context_t *new)
# rdi = old
# rsi = new

context_switch:

    # -------------------------
    # SAVE OLD CONTEXT (rdi)
    # -------------------------

    movq %r15, 0(%rdi)
    movq %r14, 8(%rdi)
    movq %r13, 16(%rdi)
    movq %r12, 24(%rdi)

    movq %rbx, 32(%rdi)
    movq %rbp, 40(%rdi)

    # skip rsp for now (we set manually in scheduler OR save it carefully)
    movq (%rsp), %rax        # return RIP (rough method)
    movq %rax, 48(%rdi)

    pushfq
    popq %rax
    movq %rax, 56(%rdi)

    # -------------------------
    # LOAD NEW CONTEXT (rsi)
    # -------------------------

    movq 0(%rsi), %r15
    movq 8(%rsi), %r14
    movq 16(%rsi), %r13
    movq 24(%rsi), %r12

    movq 32(%rsi), %rbx
    movq 40(%rsi), %rbp

    movq 56(%rsi), %rax
    pushq %rax
    popfq

    # set stack pointer
    movq 48(%rsi), %rsp

    # jump to new RIP
    movq 48(%rsi), %rax
    jmp *%rax