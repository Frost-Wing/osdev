OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(dw_main)

SECTIONS {
    . = 0xffffffff80000000;

    /* Section containing the kernel code */
    .text : {
        *(.text)
    }

    /* Section containing read-only data */
    .rodata : {
        *(.rodata)
    }

    /* Section containing initialized data */
    .data : {
        *(.data)
    }

    /* Section containing uninitialized data */
    .bss : {
        *(.bss)
    }

    /* Section for storing relocation information */
    .rela.text : {
        *(.rela.text)
    }

    /* Section for storing relocation information */
    .rela.data : {
        *(.rela.data)
    }

    /* Section for storing symbol table */
    .symtab : {
        *(.symtab)
    }

    /* Section for storing string table */
    .strtab : {
        *(.strtab)
    }

    /* Section for storing section headers */
    .shstrtab : {
        *(.shstrtab)
    }

    /* Set the start address to the base address */
    dw_main = .;
}
