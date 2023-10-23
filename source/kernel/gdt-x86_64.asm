global load_gdt
section .text
load_gdt:
    lgdt [rdi]  ; Load the GDT from the memory address pointed to by rdi
    ret