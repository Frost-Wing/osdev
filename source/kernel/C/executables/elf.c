/**
 * @file elf.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-01-02
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */
#include <executables/elf.h>
#include <memory2.h>
#include <stdint.h>
#include <heap.h>
#include <elf.h>

void elf_load_program_header(Elf64_Phdr* program_header, void* file_base_addr)
{
    if (program_header == NULL)
        return;

    Elf64_Word type = program_header->p_type;
    printf("Program Header type: %x", type);
}

void* elf_load_from_memory(void* file_base_address)
{
    if (file_base_address == NULL)
        return NULL;

    uint8_t* file_ptr = file_base_address;
    Elf64_Ehdr header = {};
    memcpy(&header, file_ptr, sizeof(Elf64_Ehdr));

    if (memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 || header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_type != ET_EXEC ||
        header.e_machine != EM_X86_64 || header.e_version != EV_CURRENT)
    {
        error("Not a valid ELF file to load!", __FILE__);
        return NULL;
    }

    printf("Parsing ELF64 file with %d PHDRs\n", header.e_phnum);
    file_ptr = file_ptr + header.e_phoff;
    size_t program_headers_size = header.e_phnum * header.e_phentsize;
    Elf64_Phdr* program_headers_start = (Elf64_Phdr*)file_ptr;
    Elf64_Phdr* program_headers_end = (Elf64_Phdr*)(file_ptr + program_headers_size);
    for (Elf64_Phdr* prog_header = program_headers_start;
         (uint8_t*)prog_header < program_headers_end;
         prog_header = (Elf64_Phdr*)(((uint8_t*)prog_header + header.e_phentsize)))
        elf_load_program_header(prog_header, file_base_address);

    return (void*)header.e_entry;
}