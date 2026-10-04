/**
 * @file fdlfcn.h
 * @brief Runtime loading and symbol lookup for ELF images.
 */
#ifndef __FDLFCN_H_
#define __FDLFCN_H_ 1

#include <executables/elf.h>
#include <heap.h>

// signifies that (for example dlysm) should look through the list of loaded so files and find the address of the given symbol
#define FLD_NEXT (void *)-1

typedef struct fdlfcn_handle {
    void *address;
    Elf64_Ehdr ehdr;
    Elf64_Shdr *shdrs;
    Elf64_Sym *symbols;
    Elf64_Rela *relocations;

    int symtab_index;
    int text_section_index;
    void *string_table_data;
    void *text_section_data;
    void *data_section_data;
    void *rodata_section_data;
    void *symtab_str_section_data;

    Elf64_Shdr *text_section_header;
    Elf64_Shdr *string_table_header;
    Elf64_Shdr *data_section_header;
    Elf64_Shdr *rodata_section_header;
    Elf64_Shdr *symtab_str_section_header;

    struct fdlfcn_handle *prev;
    struct fdlfcn_handle *next;
} fdlfcn_handle;

// immediately load sections into memory
#define FDL_IMMEDIATE 0

/**
 * @brief Load an ELF shared object from memory.
 *
 * @param filedata Pointer to the in-memory ELF image.
 * @param flags Loader options.
 * @return A handle to the loaded image, or NULL on failure.
 */
fdlfcn_handle *fdlopen(void *filedata, int flags);

/**
 * @brief Look up a symbol in a loaded image.
 *
 * @param handle Handle returned by fdlopen.
 * @param symbol_name Name of the symbol to find.
 * @return Symbol address, or NULL if it is not found.
 */
void *fdlsym(fdlfcn_handle *handle, const char *symbol_name);

/**
 * @brief Release a loaded image.
 *
 * @param handle Handle returned by fdlopen.
 * @return 0 on success, nonzero on failure.
 */
int fdlclose(fdlfcn_handle *handle);

// defines that make my life easier
#define dlopen fdlopen
#define dlsym fdlsym
#define dlclose fdlclose

#endif