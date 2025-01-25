#include <memory2.h>
#include <fdlfcn.h>
#include <stdint.h>
#include <stddef.h>
#include <heap.h>
#include <elf.h>

#define READ_FROM_MEMORY(dest, base, offset, size) memcpy(dest, (void*)( (uint8_t*)base + offset ), size)

void* fdl_load_section(void* filedata, Elf64_Shdr* section_header)
{
    void* section_data = malloc(section_header->sh_size);
    if (section_data == NULL) {
        error("fdlopen: malloc failed for section");
        return NULL;
    }

    READ_FROM_MEMORY(section_data, filedata, section_header->sh_offset, section_header->sh_size);

    return section_data;
}

int fdl_apply_relocations(fdlfcn_handle* lib, int reloc_section_index)
{
    Elf64_Ehdr* elf_header = &lib->ehdr;
    Elf64_Shdr* section_headers = lib->shdrs;
    Elf64_Sym* symbols = lib->symbols;
    Elf64_Rela* relocations = lib->relocations;

    // Get number of relocations
    int num_relocations = section_headers[reloc_section_index].sh_size / sizeof(Elf64_Rela); 

    for (int i = 0; i < num_relocations; i++) {
        Elf64_Rela* reloc = &relocations[i]; 

        // Get symbol index
        int sym_index = ELF64_R_SYM(reloc->r_info); 
        Elf64_Sym* sym = &symbols[sym_index];

        // Get relocation type
        int reloc_type = ELF64_R_TYPE(reloc->r_info);

        // Get target address
        uintptr_t target_addr = 0; 

        // Determine target address based on relocation type
        switch (reloc_type) {
            case R_X86_64_64: 
                target_addr = (uintptr_t)sym->st_value; 
                break;
            case R_X86_64_PC32: 
                target_addr = (uintptr_t)sym->st_value - (uintptr_t)(lib->address + reloc->r_offset + 4); 
                break;
            case R_X86_64_PLT32: 
                // Simplified handling - Adjust based on actual PLT implementation
                target_addr = (uintptr_t)sym->st_value; 
                break;
            case R_X86_64_GOTPCREL: 
                // Simplified handling - Adjust based on actual GOT implementation
                target_addr = (uintptr_t)sym->st_value; 
                break;
            default:
                error("fdlopen: Unsupported relocation type: %d", reloc_type);
                return -1; 
        }

        // Apply relocation
        uintptr_t* ptr = (uintptr_t*)(lib->address + reloc->r_offset); 
        *ptr += target_addr + reloc->r_addend; 
    }

    return 0; // Relocation successful
}

void* fdlsym(fdlfcn_handle* handle, const char* symbol_name)
{
    for (int i = 0; i < handle->ehdr.e_shnum; i++) {
        if (strcmp(handle->shdrs[i].sh_name, ".symtab") == 0) {
            Elf64_Shdr symtab_section = handle->shdrs[i];
            Elf64_Sym* symbols = handle->symbols;

            for (int j = 0; j < symtab_section.sh_size / sizeof(Elf64_Sym); j++) {
                char* symbol_name_str = (char*)((Elf64_Shdr*)((char*)handle->shdrs + handle->ehdr.e_shstrndx))->sh_addr + symbols[j].st_name;
                if (strcmp(symbol_name_str, symbol_name) == 0) {
                    // Handle symbol versioning (if needed)
                    // ...

                    // Calculate symbol address
                    uintptr_t symbol_address = (uintptr_t)handle->address + symbols[j].st_value; 
                    return (void*)symbol_address;
                }
            }
            break;
        }
    }

    error("fdlsym: Symbol '%s' not found", symbol_name);
    return NULL;
}

int fdlclose(fdlfcn_handle* handle)
{
    if (handle->text_section_data)
        free(handle->text_section_data);
    if (handle->data_section_data)
        free(handle->data_section_data);
    if (handle->rodata_section_data)
        free(handle->rodata_section_data);

    free(handle->shdrs);
    free(handle->symbols);
    free(handle->relocations);
    free(handle);

    return 0;
}

fdlfcn_handle* fdlopen(void* filedata, int flags)
{
    if (filedata == NULL || flags != FDL_IMMEDIATE)
        return NULL;

    fdlfcn_handle* handle = malloc(sizeof(fdlfcn_handle));
    if (handle == NULL)
    {
        error("Could not allocate memory for shared object file handle");
        return NULL;
    }
    memset(handle, 0, sizeof(fdlfcn_handle));

    // Read ELF header
    Elf64_Ehdr elf_header;
    memcpy(&elf_header, filedata, sizeof(Elf64_Ehdr));

    // Read section headers
    Elf64_Shdr* section_headers = malloc(elf_header.e_shnum * sizeof(Elf64_Shdr));
    READ_FROM_MEMORY(section_headers, filedata, elf_header.e_shoff, elf_header.e_shnum * sizeof(Elf64_Shdr));

    // Find important section indices
    int strtab_index = elf_header.e_shstrndx;
    int symtab_index = -1;
    int text_section_index = -1;
    int data_section_index = -1;
    int rodata_section_index = -1;
    int reloc_section_index = -1;

    for (int i = 0; i < elf_header.e_shnum; i++)
    {
        char* section_name = (char*)((Elf64_Shdr*)((char*)section_headers + strtab_index))->sh_addr + section_headers[i].sh_name;
        if (strcmp(section_name, ".text") == 0)
            text_section_index = i;
        else if (strcmp(section_name, ".data") == 0)
            data_section_index = i;
        else if (strcmp(section_name, ".rodata") == 0)
            rodata_section_index = i;
        else if (strcmp(section_name, ".symtab") == 0)
            symtab_index = i;
        else if (strcmp(section_name, ".strtab") == 0)
            strtab_index = i; 
        else if (strcmp(section_name, ".reloc") == 0)
            reloc_section_index = i;
    }

    // Load sections into memory
    void* text_section_data = NULL;
    void* data_section_data = NULL;
    void* rodata_section_data = NULL;

    if (text_section_index != -1)
    {
        text_section_data = fdl_load_section(filedata, &section_headers[text_section_index]);
        if (text_section_data == NULL)
        {
            free(section_headers);
            return NULL;
        }
    }

    if (data_section_index != -1)
    {
        data_section_data = fdl_load_section(filedata, &section_headers[data_section_index]);
        if (data_section_data == NULL)
        {
            if (text_section_data)
                free(text_section_data);

            free(section_headers);
            return NULL;
        }
    }

    if (rodata_section_index != -1)
    {
        rodata_section_data = fdl_load_section(filedata, &section_headers[rodata_section_index]);
        if (rodata_section_data == NULL)
        {
            if (text_section_data)
                free(text_section_data);
            if (data_section_data)
                free(data_section_data);

            free(section_headers);
            return NULL;
        }
    }

    // Read symbol table
    if (symtab_index != -1)
    {
        Elf64_Shdr symtab_section = section_headers[symtab_index];
        handle->symbols = malloc(symtab_section.sh_size);
        READ_FROM_MEMORY(handle->symbols, filedata, symtab_section.sh_offset, symtab_section.sh_size);
    } else {
        handle->symbols = NULL; 
    }

    // Read relocation entries
    if (reloc_section_index != -1) {
        Elf64_Shdr reloc_section = section_headers[reloc_section_index];
        handle->relocations = malloc(reloc_section.sh_size);
        READ_FROM_MEMORY(handle->relocations, filedata, reloc_section.sh_offset, reloc_section.sh_size);
    } else {
        handle->relocations = NULL; 
    }

    handle->address = (void*)section_headers[text_section_index].sh_addr; 

    if (fdl_apply_relocations(handle, reloc_section_index) != 0)
    {
        error("fdlopen: Relocation failed");

        if (text_section_data)
            free(text_section_data);
        if (data_section_data)
            free(data_section_data);
        if (rodata_section_data)
            free(rodata_section_data);

        free(section_headers);
        free(handle->symbols);
        free(handle->relocations);
        free(handle);
        return NULL;
    }

    handle->text_section_data = text_section_data;
    handle->text_section_header = &section_headers[text_section_index];
    handle->data_section_data = data_section_data;
    handle->data_section_header = &section_headers[data_section_index];
    handle->rodata_section_data = rodata_section_data;
    handle->rodata_section_header = &section_headers[rodata_section_index];

    handle->ehdr = elf_header;
    handle->shdrs = section_headers; 
    handle->symtab_index = symtab_index; // Store symtab index for later use

    return handle;
}
