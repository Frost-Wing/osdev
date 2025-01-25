#ifndef __FDLFCN_H_
#define __FDLFCN_H_ 1

#include <elf.h>

typedef struct
{
    void* address;
    Elf64_Ehdr ehdr;
    Elf64_Shdr* shdrs;
    Elf64_Sym* symbols;
    Elf64_Rela* relocations;

    int symtab_index;
    void* text_section_data;
    void* data_section_data;
    void* rodata_section_data;

    Elf64_Shdr* text_section_header;
    Elf64_Shdr* data_section_header;
    Elf64_Shdr* rodata_section_header;
} fdlfcn_handle;

// immediately load sections into memory
#define FDL_IMMEDIATE 0

fdlfcn_handle* fdlopen(void* filedata, int flags);
void* fdlsym(fdlfcn_handle* handle, const char* symbol_name);
int fdlclose(fdlfcn_handle* handle);

#endif