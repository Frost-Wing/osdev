#include <graphics.h>
#include <memory2.h>
#include <fdlfcn.h>
#include <stdint.h>
#include <stddef.h>
#include <elf.h>

fdlfcn_handle* global_library_handles;

#define READ_FROM_MEMORY(dest, base, offset, size) memcpy(dest, (void*)( (uint64_t)base + (uint64_t)offset ), size)

void* fdl_load_section(void* filedata, Elf64_Shdr* section_header)
{
    void* section_data = kmalloc(section_header->sh_size);
    if (section_data == NULL)
    {
        error("fdlopen: malloc failed for section", __FILE__);
        return NULL;
    }

    READ_FROM_MEMORY(section_data, filedata, section_header->sh_offset, section_header->sh_size);

    return NULL;
}

int fdl_apply_relocations(fdlfcn_handle* lib, int reloc_section_index)
{
    if (lib == NULL || reloc_section_index == -1)
        return 1;

    Elf64_Ehdr* elf_header = &lib->ehdr;
    Elf64_Shdr* section_headers = lib->shdrs;
    Elf64_Sym* symbols = lib->symbols;
    Elf64_Rela* relocations = lib->relocations;

    int num_relocations = section_headers[reloc_section_index].sh_size / sizeof(Elf64_Rela); 

    for (int i = 0; i < num_relocations; i++)
    {
        Elf64_Rela* reloc = &relocations[i]; 

        int sym_index = ELF64_R_SYM(reloc->r_info); 
        Elf64_Sym* sym = &symbols[sym_index];

        int reloc_type = ELF64_R_TYPE(reloc->r_info);
        uintptr_t target_addr = 0; 

        if (reloc_type == R_X86_64_64)
            target_addr = (uintptr_t)sym->st_value; 
        else if (reloc_type == R_X86_64_PC32)
            target_addr = (uintptr_t)sym->st_value - (uintptr_t)(lib->address + reloc->r_offset + 4);
        else if (reloc_type == R_X86_64_PLT32)
            target_addr = (uintptr_t)sym->st_value;
        else if (reloc_type == R_X86_64_GOTPCREL)
            target_addr = (uintptr_t)sym->st_value; 
        else
        {
            printf("fdlopen: Unsupported relocation type: %d: %s", reloc_type, __FILE__);
            return -1;
        }

        uintptr_t* ptr = (uintptr_t*)(lib->address + reloc->r_offset); 
        *ptr += target_addr + reloc->r_addend; 
    }

    return 0;
}

void* fdlsym(fdlfcn_handle* handle, const char* symbol_name)
{
    if (handle == NULL)
        return NULL;
    if (handle == FLD_NEXT)
    {
        for (fdlfcn_handle* entry = global_library_handles; entry != NULL; entry = entry->next)
        {
            void* addr = fdlsym(entry, symbol_name);
            if (addr != NULL)
                return addr;
        }
        printf("Error: No library loaded with symbol '%s'", symbol_name);
        return NULL;
    }

    Elf64_Sym* symbols = handle->symbols;

    int symtab_index = -1;
    for (int i = 0; i < handle->ehdr.e_shnum; i++)
    {
        if (handle->shdrs[i].sh_type == SHT_SYMTAB)
        {
            symtab_index = i;
            break;
        }
    }

    if (symtab_index == -1)
    {
        printf("Symbol '%s' not found", symbol_name);
        return NULL;
    }

    Elf64_Shdr symtab_section = handle->shdrs[symtab_index];

    for (int j = 0; j < symtab_section.sh_size / sizeof(Elf64_Sym); j++)
    {
        Elf64_Sym symbol = symbols[j];
        char* symbol_name_str = (char*)((uint64_t)handle->symtab_str_section_data + symbol.st_name);
        if (strcmp(symbol_name_str, symbol_name) == 0 && ELF64_ST_BIND(symbol.st_info) != STB_LOCAL && symbol.st_shndx == handle->text_section_index)
        {
            uintptr_t symbol_address = (uintptr_t)handle->text_section_data + symbol.st_value - handle->text_section_header->sh_offset;
            if (symbol.st_shndx != SHN_UNDEF)
                return (void*)symbol_address;
        }
    }

    printf("fdlsym: Symbol '%s' not found", symbol_name);
    return NULL;
}

int fdlclose(fdlfcn_handle* handle)
{
    if (handle == NULL)
        return 1;

    for (fdlfcn_handle* entry = global_library_handles; entry != NULL; entry = entry->next)
    {
        if (entry != handle)
            continue;

        if (entry->prev != NULL)
            entry->prev->next = entry->next;

        if (entry->next != NULL)
            entry->next->prev = entry->prev;

        if (entry == global_library_handles)
            global_library_handles = NULL;

        break;
    }

    return 0; /// TODO: somehow all the free calls cause a gp fault, fix that

    if (handle->text_section_data != NULL)
        kfree(handle->text_section_data);
    if (handle->data_section_data != NULL)
        kfree(handle->data_section_data);
    if (handle->rodata_section_data != NULL)
        kfree(handle->rodata_section_data);
    if (handle->string_table_data != NULL)
        kfree(handle->string_table_data);
    if (handle->symtab_str_section_data != NULL)
        kfree(handle->symtab_str_section_data);
    if (handle->shdrs != NULL)
        kfree(handle->shdrs);
    if (handle->symbols != NULL)
        kfree(handle->symbols);
    if (handle->relocations != NULL)
        kfree(handle->relocations);

    kfree(handle);

    return 0;
}

fdlfcn_handle* fdlopen(void* filedata, int flags)
{
    if (filedata == NULL || flags != FDL_IMMEDIATE)
        return NULL;

    fdlfcn_handle* handle = kmalloc(sizeof(fdlfcn_handle));
    if (handle == NULL)
    {
        error("Could not allocate memory for shared object file handle", __FILE__);
        return NULL;
    }
    memset(handle, 0, sizeof(fdlfcn_handle));

    Elf64_Ehdr elf_header;
    memcpy(&elf_header, filedata, sizeof(Elf64_Ehdr));

    if (memcmp(&elf_header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 || elf_header.e_ident[EI_CLASS] != ELFCLASS64 || elf_header.e_type != ET_DYN ||
        elf_header.e_machine != EM_X86_64 || elf_header.e_version != EV_CURRENT)
    {
        error("Not a valid .so file", __FILE__);
        kfree(handle);
        return NULL;
    }

    Elf64_Shdr* section_headers = kmalloc(elf_header.e_shnum * sizeof(Elf64_Shdr));
    if (section_headers == NULL)
    {
        kfree(handle);
        return NULL;
    }
    READ_FROM_MEMORY(section_headers, filedata, elf_header.e_shoff, elf_header.e_shnum * sizeof(Elf64_Shdr));

    int strtab_index = elf_header.e_shstrndx;
    int symtab_index = -1;
    int text_section_index = -1;
    int data_section_index = -1;
    int rodata_section_index = -1;
    int reloc_section_index = -1;
    int symtab_str_section_index = -1;

    void* strtableAddr = fdl_load_section(filedata, &section_headers[strtab_index]);
    for (int i = 0; i < elf_header.e_shnum; i++)
    {
        char* section_name = (char*)strtableAddr + section_headers[i].sh_name;
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

    void* text_section_data = NULL;
    void* data_section_data = NULL;
    void* rodata_section_data = NULL;
    void* symtab_str_section_data = NULL;

    if (text_section_index != -1)
    {
        text_section_data = fdl_load_section(filedata, &section_headers[text_section_index]);
        if (text_section_data == NULL)
        {
            kfree(section_headers);
            return NULL;
        }
    }

    if (data_section_index != -1)
    {
        data_section_data = fdl_load_section(filedata, &section_headers[data_section_index]);
        if (data_section_data == NULL)
        {
            if (text_section_data)
                kfree(text_section_data);

            kfree(section_headers);
            return NULL;
        }
    }

    if (rodata_section_index != -1)
    {
        rodata_section_data = fdl_load_section(filedata, &section_headers[rodata_section_index]);
        if (rodata_section_data == NULL)
        {
            if (text_section_data)
                kfree(text_section_data);
            if (data_section_data)
                kfree(data_section_data);

            kfree(section_headers);
            return NULL;
        }
    }

    if (symtab_index != -1)
    {
        Elf64_Shdr symtab_section = section_headers[symtab_index];
        handle->symbols = kmalloc(symtab_section.sh_size);
        READ_FROM_MEMORY(handle->symbols, filedata, symtab_section.sh_offset, symtab_section.sh_size);
        symtab_str_section_index = symtab_section.sh_link;
        if (symtab_str_section_index != -1)
            symtab_str_section_data = fdl_load_section(filedata, &section_headers[symtab_str_section_index]);
    }
    else
        handle->symbols = NULL; 

    if (reloc_section_index != -1)
    {
        Elf64_Shdr reloc_section = section_headers[reloc_section_index];
        handle->relocations = kmalloc(reloc_section.sh_size);
        READ_FROM_MEMORY(handle->relocations, filedata, reloc_section.sh_offset, reloc_section.sh_size);
    }
    else
        handle->relocations = NULL; 

    handle->address = text_section_data;
    handle->text_section_data = text_section_data;
    handle->text_section_index = text_section_index;
    handle->text_section_header = &section_headers[text_section_index];
    handle->string_table_data = strtableAddr;
    handle->string_table_header = &section_headers[strtab_index];
    handle->data_section_data = data_section_data;
    handle->data_section_header = &section_headers[data_section_index];
    handle->rodata_section_data = rodata_section_data;
    handle->rodata_section_header = &section_headers[rodata_section_index];
    handle->symtab_str_section_data = symtab_str_section_data;
    handle->symtab_str_section_header = &section_headers[symtab_str_section_index];
    handle->string_table_data = strtableAddr;
    handle->ehdr = elf_header;
    handle->shdrs = section_headers;
    handle->symtab_index = symtab_index;
    handle->next = NULL;
    handle->prev = NULL;

    if (fdl_apply_relocations(handle, reloc_section_index) != 0 && reloc_section_index != -1)
    {
        error("fdlopen: Relocation failed", __FILE__);

        if (text_section_data)
            kfree(text_section_data);
        if (data_section_data)
            kfree(data_section_data);
        if (rodata_section_data)
            kfree(rodata_section_data);
        if (strtableAddr)
            kfree(strtableAddr);
        if (symtab_str_section_data)
            kfree(symtab_str_section_data);

        kfree(section_headers);
        kfree(handle->symbols);
        kfree(handle->relocations);
        kfree(handle);
        return NULL;
    }

    if (global_library_handles == NULL)
    {
        global_library_handles = handle;
    }
    else
    {
        global_library_handles->prev = handle;
        handle->next = global_library_handles;
        global_library_handles = handle;
    }

    info("fdlopen: Successfully loaded .so file", __FILE__);

    return handle;
}
