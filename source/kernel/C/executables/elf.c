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

static uint32_t* elf_vfs_pos_ptr(vfs_file_t* file)
{
    if (!file || !file->mnt)
        return NULL;

    switch (file->mnt->type) {
        case FS_FAT16:
            return &file->f.fat16.pos;
        case FS_FAT32:
            return &file->f.fat32.pos;
        case FS_ISO9660:
            return &file->f.iso9660.pos;
        default:
            return NULL;
    }
}

static int elf_vfs_seek(vfs_file_t* file, uint32_t offset)
{
    uint32_t* pos = elf_vfs_pos_ptr(file);
    if (!pos)
        return -1;

    *pos = offset;
    return 0;
}

static int elf_vfs_read_exact(vfs_file_t* file, uint32_t offset, void* buf, uint32_t size)
{
    if (elf_vfs_seek(file, offset) != 0)
        return -1;

    int rd = vfs_read(file, (uint8_t*)buf, size);
    return (rd >= 0 && (uint32_t)rd == size) ? 0 : -1;
}

static uint64_t elf_runtime_addr_for_offset(Elf64_Phdr* headers, uint16_t phnum, uint64_t file_offset)
{
    for (uint16_t i = 0; i < phnum; ++i) {
        Elf64_Phdr* ph = &headers[i];
        if (ph->p_type != PT_LOAD || ph->p_filesz == 0)
            continue;

        uint64_t seg_start = ph->p_offset;
        uint64_t seg_end = ph->p_offset + ph->p_filesz;
        if (file_offset >= seg_start && file_offset < seg_end)
            return ph->p_vaddr + (file_offset - ph->p_offset);
    }

    return 0;
}

static int elf_validate_header(const Elf64_Ehdr* header, uint64_t file_size)
{
    if (memcmp(&header->e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 || header->e_ident[EI_CLASS] != ELFCLASS64 ||
        header->e_ident[EI_DATA] != ELFDATA2LSB || header->e_type != ET_EXEC ||
        header->e_machine != EM_X86_64 || header->e_version != EV_CURRENT)
    {
        error("Not a valid ELF file to load!", __FILE__);
        return -1;
    }

    if (header->e_phoff + ((uint64_t)header->e_phnum * header->e_phentsize) > file_size ||
        header->e_phentsize != sizeof(Elf64_Phdr)) {
        eprintf("elf: invalid program header table");
        return -1;
    }

    return 0;
}

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

    // printf("Loaded user segment -> vaddr=%x size=%u flags=%x", segment, ph->p_memsz, ph->p_flags);
    return 0;
}

static int elf_map_program_header_from_vfs(Elf64_Phdr* ph, vfs_file_t* file, uint64_t file_size)
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

    if (ph->p_filesz != 0) {
        if (elf_vfs_read_exact(file, (uint32_t)ph->p_offset, (void*)ph->p_vaddr, (uint32_t)ph->p_filesz) != 0) {
            eprintf("elf: failed to read segment");
            return -1;
        }
    }

    if (ph->p_memsz > ph->p_filesz)
        memset((uint8_t*)ph->p_vaddr + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);

    return 0;
}

void* elf_load_from_memory_ex(void* file_base_address, uint64_t file_size, elf_image_info_t* info)
{
    if (file_base_address == NULL || file_size < sizeof(Elf64_Ehdr))
        return NULL;

    uint8_t* file_ptr = file_base_address;
    Elf64_Ehdr header = {};
    memcpy(&header, file_ptr, sizeof(Elf64_Ehdr));

    if (elf_validate_header(&header, file_size) != 0)
        return NULL;

    // printf("Parsing ELF64 file with %d PHDRs\n", header.e_phnum);

    Elf64_Phdr* program_headers_start = (Elf64_Phdr*)((uint8_t*)file_base_address + header.e_phoff);

    for (uint16_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr* prog_header = (Elf64_Phdr*)((uint8_t*)program_headers_start + ((uint64_t)i * header.e_phentsize));
        if (elf_map_program_header(prog_header, file_base_address, file_size) != 0) {
            return NULL;
        }
    }

    if (info) {
        info->entry = header.e_entry;
        info->phdr_addr = elf_runtime_addr_for_offset(program_headers_start, header.e_phnum, header.e_phoff);
        info->phentsize = header.e_phentsize;
        info->phnum = header.e_phnum;
    }

    return (void*)header.e_entry;
}

void* elf_load_from_memory(void* file_base_address, uint64_t file_size)
{
    return elf_load_from_memory_ex(file_base_address, file_size, NULL);
}

void* elf_load_from_vfs_ex(const char* path, elf_image_info_t* info)
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

    Elf64_Ehdr header = {};
    if (elf_vfs_read_exact(&file, 0, &header, sizeof(header)) != 0) {
        eprintf("elf: failed to read header");
        vfs_close(&file);
        return NULL;
    }

    if (elf_validate_header(&header, size) != 0) {
        vfs_close(&file);
        return NULL;
    }

    uint64_t phdr_bytes = (uint64_t)header.e_phnum * header.e_phentsize;
    Elf64_Phdr* program_headers = kmalloc(phdr_bytes);
    if (!program_headers) {
        eprintf("elf: failed to allocate program header table");
        vfs_close(&file);
        return NULL;
    }

    if (elf_vfs_read_exact(&file, (uint32_t)header.e_phoff, program_headers, (uint32_t)phdr_bytes) != 0) {
        eprintf("elf: failed to read program headers");
        kfree(program_headers);
        vfs_close(&file);
        return NULL;
    }

    for (uint16_t i = 0; i < header.e_phnum; ++i) {
        if (elf_map_program_header_from_vfs(&program_headers[i], &file, size) != 0) {
            kfree(program_headers);
            vfs_close(&file);
            return NULL;
        }
    }

    if (info) {
        info->entry = header.e_entry;
        info->phdr_addr = elf_runtime_addr_for_offset(program_headers, header.e_phnum, header.e_phoff);
        info->phentsize = header.e_phentsize;
        info->phnum = header.e_phnum;
    }

    kfree(program_headers);
    vfs_close(&file);
    void* entry = (void*)header.e_entry;
    return entry;
}

void* elf_load_from_vfs(const char* path)
{
    return elf_load_from_vfs_ex(path, NULL);
}
