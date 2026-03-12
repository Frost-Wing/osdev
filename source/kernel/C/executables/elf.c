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
#include <paging.h>

static int elf_map_program_header(Elf64_Phdr* ph, void* file_base, uint64_t file_size)
{
    if (ph->p_type != PT_LOAD)
        return 0;

    if (ph->p_offset + ph->p_filesz > file_size || ph->p_memsz < ph->p_filesz) {
        eprintf("elf: invalid PT_LOAD bounds");
        return -1;
    }

    uint64_t seg_start = ph->p_vaddr & ~0xFFFULL;
    uint64_t seg_end = (ph->p_vaddr + ph->p_memsz + 0xFFFULL) & ~0xFFFULL;

    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    if (ph->p_flags & PF_W)
        page_flags |= PAGE_RW;
    if (!(ph->p_flags & PF_X))
        page_flags |= PAGE_NX;

    for (uint64_t page = seg_start; page < seg_end; page += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        map_user_page(page, phys, page_flags);
    }

    void* segment = (void*)ph->p_vaddr;

    memcpy(segment, (uint8_t*)file_base + ph->p_offset, ph->p_filesz);

    if (ph->p_memsz > ph->p_filesz) {
        memset((uint8_t*)segment + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
    }

    printf("Loaded user segment -> vaddr=%x size=%u flags=%x", segment, ph->p_memsz, ph->p_flags);
    return 0;
}

void* elf_load_from_memory(void* file_base_address, uint64_t file_size)
{
    if (file_base_address == NULL || file_size < sizeof(Elf64_Ehdr))
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

    if (header.e_phoff + ((uint64_t)header.e_phnum * header.e_phentsize) > file_size ||
        header.e_phentsize != sizeof(Elf64_Phdr)) {
        eprintf("elf: invalid program header table");
        return NULL;
    }

    printf("Parsing ELF64 file with %d PHDRs\n", header.e_phnum);

    Elf64_Phdr* program_headers_start = (Elf64_Phdr*)((uint8_t*)file_base_address + header.e_phoff);

    for (uint16_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr* prog_header = (Elf64_Phdr*)((uint8_t*)program_headers_start + ((uint64_t)i * header.e_phentsize));
        if (elf_map_program_header(prog_header, file_base_address, file_size) != 0) {
            return NULL;
        }
    }

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

    void* entry = elf_load_from_memory(image, size);
    kfree(image);
    return entry;
}
