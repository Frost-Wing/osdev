global syscall_entry
extern syscall_handler
extern kernel_stack_top
extern userland_should_return_kernel
extern userland_resume_rsp
extern userland_resume_rip
extern userland_resume_rbx
extern userland_resume_rbp
extern userland_resume_r12
extern userland_resume_r13
extern userland_resume_r14
extern userland_resume_r15

section .bss
align 8
saved_user_rsp: resq 1
saved_user_rbx: resq 1

section .text

syscall_entry:
    swapgs

    ; Preserve userland RBX and RSP across SYSCALL.
    ; NOTE: single-slot scratch is fine for current single-core/single-threaded userspace,
    ; but this should eventually be per-CPU/per-thread storage.
    mov [rel saved_user_rbx], rbx
    mov [rel saved_user_rsp], rsp

    ; Switch to kernel stack
    mov rsp, [rel kernel_stack_top]

    ; Build FULL iret frame manually
    push 0x23        ; SS (user data)
    mov rbx, [rel saved_user_rsp]
    push rbx         ; RSP (user stack)
    push r11         ; RFLAGS
    push 0x1B        ; CS (user code)
    push rcx         ; RIP

    ; Save registers
    push rax
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

    ; Call C handler
    mov rdi, rsp
    call syscall_handler

    ; Restore registers
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop rax

    ; Restore userland RBX prior to iretq.
    mov rbx, [rel saved_user_rbx]

    cmp byte [rel userland_should_return_kernel], 0
    je .return_to_user

    mov rbx, [rel userland_resume_rbx]
    mov rbp, [rel userland_resume_rbp]
    mov r12, [rel userland_resume_r12]
    mov r13, [rel userland_resume_r13]
    mov r14, [rel userland_resume_r14]
    mov r15, [rel userland_resume_r15]
    mov rsp, [rel userland_resume_rsp]
    mov rax, [rel userland_resume_rip]
    jmp rax

.return_to_user:

    ; Return to user
    swapgs
    iretq
