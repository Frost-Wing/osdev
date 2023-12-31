/* Tell the linker that we want an x86_64 ELF64 output file */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
 
/* We want the symbol main to be our entry point */
ENTRY(fdm_main)
 
SECTIONS
{
    . = 0x100000;

    .text : {
        *(.text)       /* Code */
        *(.rodata)     /* Read-only data */
    }

    .data : {
        *(.data)       /* Data */
    }

    .bss : {
        *(.bss)        /* Uninitialized data */
    }

    . = ALIGN(4096);   /* Align the following section to a page boundary */

    /* Other sections as needed */

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}