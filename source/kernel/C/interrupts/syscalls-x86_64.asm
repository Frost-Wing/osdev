global syscall_entry
extern syscall_handler_syscall
extern kernel_stack_top

section .text

syscall_entry:
    swapgs

    ; Save user RSP
    mov rbx, rsp

    ; Switch to kernel stack
    mov rsp, kernel_stack_top

    ; Build FULL iret frame manually
    push 0x23        ; SS (user data)
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
    call syscall_handler_syscall

    ; Restore registers
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop rax

    ; Return to user
    swapgs
    iretq