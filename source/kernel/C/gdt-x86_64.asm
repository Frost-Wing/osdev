; gdt.asm
; Load GDT from the address in RDI

global load_gdt

section .text
load_gdt:
    lgdt [rdi]   ; Load the GDT from pointer in RDI
    ret
