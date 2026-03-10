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
#include <fdlfcn.h>
#include <memory.h>
#include <stdint.h>
#include <heap.h>
#include <filesystems/vfs.h>

void elf_load_program_header(Elf64_Phdr* ph, void* file_base)
{
    if (ph->p_type != 1)
        return;

    void* segment = (void*)ph->p_vaddr;

    memcpy(
        segment,
        (uint8_t*)file_base + ph->p_offset,
        ph->p_filesz
    );

    if (ph->p_memsz > ph->p_filesz)
    {
        memset(
            (uint8_t*)segment + ph->p_filesz,
            0,
            ph->p_memsz - ph->p_filesz
        );
    }

    printf("Loaded segment -> vaddr=%u size=%u", segment, ph->p_memsz);
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
    
    // enter_userland();
    return (void*)header.e_entry;
}

void* elf_load_from_vfs(const char* path)
{
    if (!path)
        return NULL;

    vfs_file_t file;
    if (vfs_open(path, VFS_RDONLY, &file) != 0) {
        eprintf("elf: failed to open %s", path);
        return NULL;
    }

    uint32_t size = 0;
    switch (file.mnt->type) {
        case FS_FAT16:
            size = file.f.fat16.entry.filesize;
            break;
        case FS_FAT32:
            size = file.f.fat32.entry.file_size;
            break;
        case FS_ISO9660:
            size = file.f.iso9660.entry.size;
            break;
        default:
            vfs_close(&file);
            return NULL;
    }

    if (size == 0) {
        vfs_close(&file);
        return NULL;
    }

    void* image = kmalloc(size);
    if (!image) {
        vfs_close(&file);
        return NULL;
    }

    int rd = vfs_read(&file, (uint8_t*)image, size);
    vfs_close(&file);

    if (rd < 0 || (uint32_t)rd != size) {
        kfree(image);
        return NULL;
    }

    return elf_load_from_memory(image);
}
