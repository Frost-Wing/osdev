/* Tell the linker that we want an aarch32 ELF output file */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)

/* We want the symbol _start to be our entry point */
ENTRY(_start)

/* Define the program headers we want so the bootloader gives us the right */
/* MMU permissions */
PHDRS
{
    text PT_LOAD FLAGS(5 | 6) ; /* Execute + Read */
    rodata PT_LOAD FLAGS(4) ; /* Read only */
    data PT_LOAD FLAGS(2 | 4) ; /* Write + Read */
    dynamic PT_DYNAMIC FLAGS(2 | 4) ; /* Dynamic PHDR for relocations */
}

SECTIONS
{
    /* We wanna be placed in a suitable address space for AArch32 */
    . = 0x8000;

    .text : {
        *(.text .text.*)
    } :text

    . += CONSTANT(MAXPAGESIZE);

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . += CONSTANT(MAXPAGESIZE);

    .data : {
        *(.data .data.*)
    } :data

    .dynamic : {
        *(.dynamic)
    } :data :dynamic

    /* NOTE: .bss needs to be the last thing mapped to :data, otherwise lots of */
    /* unnecessary zeros will be written to the binary. */
    /* If you need, for example, .init_array and .fini_array, those should be placed */
    /* above this. */
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /* Discard .note.* and .eh_frame since they may cause issues on some hosts. */
    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }
}
