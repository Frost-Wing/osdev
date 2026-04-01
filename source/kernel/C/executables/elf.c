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
#include <userland.h>

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

    if (file->mnt->type == FS_FAT32) {
        fat32_file_t* fat32 = &file->f.fat32;
        uint32_t cluster_size = fat32->fs->sectors_per_cluster * FAT32_SECTOR_SIZE;
        uint32_t cluster = fat32->start_cluster;
        uint32_t steps = cluster_size ? (offset / cluster_size) : 0;

        while (steps > 0 && cluster < FAT32_CLUSTER_EOC) {
            cluster = fat32_read_fat(fat32->fs, cluster);
            steps--;
        }

        fat32->current_cluster = cluster;
    }

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

static int elf_vfs_read_exact_path(const char* path, uint32_t offset, void* buf, uint32_t size)
{
    vfs_file_t file;
    if (vfs_open(path, VFS_RDONLY, &file) != 0)
        return -1;

    int rc = elf_vfs_read_exact(&file, offset, buf, size);
    vfs_close(&file);
    return rc;
}

static void elf_record_tls_segment(const Elf64_Phdr* ph, elf_image_info_t* info)
{
    if (!info || !ph || ph->p_type != PT_TLS)
        return;

    info->tls_offset = ph->p_offset;
    info->tls_filesz = ph->p_filesz;
    info->tls_memsz = ph->p_memsz;
    info->tls_align = ph->p_align;
}

static int elf_load_tls_template_from_memory(void* file_base_address, uint64_t file_size, elf_image_info_t* info)
{
    if (!info || info->tls_filesz == 0)
        return 0;

    if (info->tls_offset + info->tls_filesz > file_size) {
        eprintf("elf: invalid PT_TLS bounds");
        return -1;
    }

    info->tls_template = kmalloc(info->tls_filesz);
    if (!info->tls_template) {
        eprintf("elf: failed to allocate TLS template");
        return -1;
    }

    memcpy(info->tls_template, (uint8_t*)file_base_address + info->tls_offset, info->tls_filesz);
    return 0;
}

static int elf_load_tls_template_from_vfs(const char* path, elf_image_info_t* info)
{
    if (!info || info->tls_filesz == 0)
        return 0;

    info->tls_template = kmalloc(info->tls_filesz);
    if (!info->tls_template) {
        eprintf("elf: failed to allocate TLS template");
        return -1;
    }

    if (elf_vfs_read_exact_path(path, (uint32_t)info->tls_offset, info->tls_template, (uint32_t)info->tls_filesz) != 0) {
        eprintf("elf: failed to read TLS template");
        kfree(info->tls_template);
        info->tls_template = NULL;
        return -1;
    }

    return 0;
}

typedef uint64_t (*elf_ifunc_resolver_t)(void);

static uint64_t elf_symbol_runtime_value(const Elf64_Sym* symtab, uint64_t sym_count, uint32_t sym_index, uint64_t load_bias)
{
    if (!symtab || sym_count == 0 || sym_index >= sym_count)
        return 0;

    const Elf64_Sym* sym = &symtab[sym_index];
    if (sym->st_shndx == SHN_UNDEF)
        return 0;

    return load_bias + sym->st_value;
}

static int elf_apply_relocation_entries(const Elf64_Rela* relocs,
                                        uint64_t reloc_count,
                                        const Elf64_Sym* symtab,
                                        uint64_t sym_count,
                                        uint64_t load_bias)
{
    if (!relocs || reloc_count == 0)
        return 0;

    for (uint64_t i = 0; i < reloc_count; ++i) {
        const Elf64_Rela* reloc = &relocs[i];
        uint32_t reloc_type = ELF64_R_TYPE(reloc->r_info);

        if (reloc_type != R_X86_64_RELATIVE)
            continue;

        uint64_t* target = (uint64_t*)(load_bias + reloc->r_offset);
        if (!target) {
            eprintf("elf: invalid relocation target");
            return -1;
        }

        *target = load_bias + (uint64_t)reloc->r_addend;
    }

    for (uint64_t i = 0; i < reloc_count; ++i) {
        const Elf64_Rela* reloc = &relocs[i];
        uint32_t reloc_type = ELF64_R_TYPE(reloc->r_info);
        uint32_t sym_index = ELF64_R_SYM(reloc->r_info);
        uint64_t* target = (uint64_t*)(load_bias + reloc->r_offset);
        uint64_t sym_value = elf_symbol_runtime_value(symtab, sym_count, sym_index, load_bias);

        if (!target) {
            eprintf("elf: invalid relocation target");
            return -1;
        }

        switch (reloc_type) {
            case R_X86_64_RELATIVE:
                break;

            case R_X86_64_IRELATIVE: {
                elf_ifunc_resolver_t resolver = (elf_ifunc_resolver_t)(load_bias + (uint64_t)reloc->r_addend);
                *target = resolver ? resolver() : 0;
                break;
            }

            case R_X86_64_64:
                *target = sym_value + (uint64_t)reloc->r_addend;
                break;

            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
                *target = sym_value + (uint64_t)reloc->r_addend;
                break;

            default:
                eprintf("elf: unsupported reloc type=%u sym=%u off=%x add=%x",
                        reloc_type,
                        sym_index,
                        reloc->r_offset,
                        reloc->r_addend);
                return -1;
        }
    }

    return 0;
}

static uint64_t elf_compute_load_bias(const Elf64_Ehdr* header, const Elf64_Phdr* headers)
{
    if (!header || !headers)
        return 0;

    uint64_t lowest_vaddr = UINT64_MAX;
    for (uint16_t i = 0; i < header->e_phnum; ++i) {
        const Elf64_Phdr* ph = &headers[i];
        if (ph->p_type != PT_LOAD)
            continue;
        if (ph->p_vaddr < lowest_vaddr)
            lowest_vaddr = ph->p_vaddr;
    }

    if (lowest_vaddr == UINT64_MAX)
        return 0;

    if (header->e_type == ET_DYN) {
        uint64_t mapped_base = USER_CODE_VADDR;
        return mapped_base - lowest_vaddr;
    }

    return 0;
}

static int elf_parse_dynamic_relocations(uint64_t load_bias,
                                         const Elf64_Phdr* headers,
                                         uint16_t phnum,
                                         const Elf64_Rela** relocs,
                                         uint64_t* reloc_count,
                                         const Elf64_Sym** symtab,
                                         uint64_t* sym_count)
{
    if (!headers || !relocs || !reloc_count || !symtab || !sym_count)
        return -1;

    const Elf64_Dyn* dynamic = NULL;
    uint64_t dynamic_count = 0;
    for (uint16_t i = 0; i < phnum; ++i) {
        if (headers[i].p_type != PT_DYNAMIC)
            continue;
        dynamic = (const Elf64_Dyn*)(load_bias + headers[i].p_vaddr);
        dynamic_count = headers[i].p_memsz / sizeof(Elf64_Dyn);
        break;
    }

    if (!dynamic || dynamic_count == 0)
        return 0;

    uint64_t rela_ptr = 0;
    uint64_t rela_sz = 0;
    uint64_t rela_ent = sizeof(Elf64_Rela);
    uint64_t sym_ptr = 0;
    uint64_t sym_ent = sizeof(Elf64_Sym);
    uint64_t hash_ptr = 0;

    for (uint64_t i = 0; i < dynamic_count; ++i) {
        const Elf64_Dyn* dyn = &dynamic[i];
        if (dyn->d_tag == DT_NULL)
            break;
        switch (dyn->d_tag) {
            case DT_RELA:
                rela_ptr = dyn->d_un.d_ptr;
                break;
            case DT_RELASZ:
                rela_sz = dyn->d_un.d_val;
                break;
            case DT_RELAENT:
                rela_ent = dyn->d_un.d_val;
                break;
            case DT_SYMTAB:
                sym_ptr = dyn->d_un.d_ptr;
                break;
            case DT_SYMENT:
                sym_ent = dyn->d_un.d_val;
                break;
            case DT_HASH:
                hash_ptr = dyn->d_un.d_ptr;
                break;
            default:
                break;
        }
    }

    if (rela_ptr == 0 || rela_sz == 0)
        return 0;
    if (rela_ent != sizeof(Elf64_Rela) || (rela_sz % rela_ent) != 0) {
        eprintf("elf: invalid DT_RELA table");
        return -1;
    }
    if (sym_ent != sizeof(Elf64_Sym)) {
        eprintf("elf: invalid DT_SYMENT");
        return -1;
    }

    *relocs = (const Elf64_Rela*)(load_bias + rela_ptr);
    *reloc_count = rela_sz / rela_ent;
    *symtab = (const Elf64_Sym*)(sym_ptr ? (load_bias + sym_ptr) : 0);
    *sym_count = 0;

    if (hash_ptr) {
        const uint32_t* hash = (const uint32_t*)(load_bias + hash_ptr);
        *sym_count = hash[1];
    }

    return 0;
}

static int elf_apply_runtime_relocations(uint64_t load_bias, const Elf64_Phdr* headers, uint16_t phnum)
{
    const Elf64_Rela* relocs = NULL;
    uint64_t reloc_count = 0;
    const Elf64_Sym* symtab = NULL;
    uint64_t sym_count = 0;

    if (elf_parse_dynamic_relocations(load_bias, headers, phnum, &relocs, &reloc_count, &symtab, &sym_count) != 0)
        return -1;

    return elf_apply_relocation_entries(relocs, reloc_count, symtab, sym_count, load_bias);
}

static uint64_t elf_runtime_addr_for_offset(Elf64_Phdr* headers, uint16_t phnum, uint64_t file_offset, uint64_t load_bias)
{
    for (uint16_t i = 0; i < phnum; ++i) {
        Elf64_Phdr* ph = &headers[i];
        if (ph->p_type != PT_LOAD || ph->p_filesz == 0)
            continue;

        uint64_t seg_start = ph->p_offset;
        uint64_t seg_end = ph->p_offset + ph->p_filesz;
        if (file_offset >= seg_start && file_offset < seg_end)
            return load_bias + ph->p_vaddr + (file_offset - ph->p_offset);
    }

    return 0;
}

static void elf_log_load_progress(uint16_t current, uint16_t total, Elf64_Phdr* ph)
{
    if (!total)
        return;

    uint32_t pct = ((uint32_t)(current + 1) * 100U) / total;
    uint32_t filled = (pct * 20U) / 100U;
    char bar[21];

    for (uint32_t i = 0; i < 20; ++i)
        bar[i] = (i < filled) ? '#' : '-';
    bar[20] = '\0';

    printf(blue_color "elf: [%s] %u% (%02u/%02u) type=%u vaddr=%x off=%x" reset_color,
           bar,
           pct,
           (uint32_t)(current + 1),
           (uint32_t)total,
           ph ? ph->p_type : 0,
           ph ? ph->p_vaddr : 0,
           ph ? ph->p_offset : 0);
}


static uint64_t elf_stage_phdrs_for_user(Elf64_Phdr* headers, uint64_t phdr_bytes)
{
    if (!headers || phdr_bytes == 0)
        return 0;

    uint64_t base = USER_PHDR_VADDR;
    uint64_t aligned = (phdr_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint64_t off = 0; off < aligned; off += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        map_user_page(base + off, phys, USER_DATA_FLAGS);
    }

    memset((void*)base, 0, aligned);
    memcpy((void*)base, headers, phdr_bytes);
    return base;
}

static int elf_validate_header(const Elf64_Ehdr* header, uint64_t file_size)
{
    if (memcmp(&header->e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 || header->e_ident[EI_CLASS] != ELFCLASS64 ||
        header->e_ident[EI_DATA] != ELFDATA2LSB || (header->e_type != ET_EXEC && header->e_type != ET_DYN) ||
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

static int elf_map_program_header(Elf64_Phdr* ph, void* file_base, uint64_t file_size, uint64_t load_bias)
{
    if (ph->p_type != PT_LOAD)
        return 0;

    if (ph->p_offset + ph->p_filesz > file_size || ph->p_memsz < ph->p_filesz) {
        eprintf("elf: invalid PT_LOAD bounds");
        return -1;
    }

    uint64_t seg_start = (load_bias + ph->p_vaddr) & ~0xFFFULL;
    uint64_t seg_end = (load_bias + ph->p_vaddr + ph->p_memsz + 0xFFFULL) & ~0xFFFULL;

    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    if (ph->p_flags & PF_W)
        page_flags |= PAGE_RW;
    if (!(ph->p_flags & PF_X))
        page_flags |= PAGE_NX;

    for (uint64_t page = seg_start; page < seg_end; page += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        map_user_page(page, phys, page_flags);
    }

    void* segment = (void*)(load_bias + ph->p_vaddr);

    memcpy(segment, (uint8_t*)file_base + ph->p_offset, ph->p_filesz);

    if (ph->p_memsz > ph->p_filesz) {
        memset((uint8_t*)segment + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
    }

    // printf("Loaded user segment -> vaddr=%x size=%u flags=%x", segment, ph->p_memsz, ph->p_flags);
    return 0;
}

static int elf_map_program_header_from_vfs(Elf64_Phdr* ph, const char* path, uint64_t file_size, uint16_t seg_index, uint64_t load_bias)
{
    if (ph->p_type != PT_LOAD)
        return 0;

    if (ph->p_offset + ph->p_filesz > file_size || ph->p_memsz < ph->p_filesz) {
        eprintf("elf: invalid PT_LOAD bounds");
        return -1;
    }

    uint64_t seg_start = (load_bias + ph->p_vaddr) & ~0xFFFULL;
    uint64_t seg_end = (load_bias + ph->p_vaddr + ph->p_memsz + 0xFFFULL) & ~0xFFFULL;

    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    if (ph->p_flags & PF_W)
        page_flags |= PAGE_RW;
    if (!(ph->p_flags & PF_X))
        page_flags |= PAGE_NX;

    for (uint64_t page = seg_start; page < seg_end; page += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        map_user_page(page, phys, page_flags);
    }

    printf("elf: seg %u off=%x vaddr=%x filesz=%u memsz=%u",
           seg_index,
           ph->p_offset,
           load_bias + ph->p_vaddr,
           ph->p_filesz,
           ph->p_memsz);

    if (ph->p_filesz != 0) {
        vfs_file_t file;
        if (vfs_open(path, VFS_RDONLY, &file) != 0) {
            eprintf("elf: seg %u reopen failed", seg_index);
            return -1;
        }

        if (elf_vfs_seek(&file, (uint32_t)ph->p_offset) != 0) {
            vfs_close(&file);
            eprintf("elf: seg %u seek failed", seg_index);
            return -1;
        }

        uint8_t chunk[4096];
        uint64_t copied = 0;
        while (copied < ph->p_filesz) {
            uint32_t remaining = (uint32_t)(ph->p_filesz - copied);
            uint32_t want = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
            int rd = vfs_read(&file, chunk, want);
            if (rd < 0 || (uint32_t)rd != want) {
                vfs_close(&file);
                eprintf("elf: seg %u short read off=%x want=%u got=%d copied=%u/%u",
                        seg_index,
                        ph->p_offset + copied,
                        want,
                        rd,
                        copied,
                        ph->p_filesz);
                return -1;
            }

            memcpy((void*)(load_bias + ph->p_vaddr + copied), chunk, want);
            copied += want;
        }

        vfs_close(&file);
        if (copied != ph->p_filesz) {
            eprintf("elf: seg %u copy mismatch copied=%u expected=%u",
                    seg_index,
                    copied,
                    ph->p_filesz);
            return -1;
        }
    }

    if (ph->p_memsz > ph->p_filesz)
        memset((uint8_t*)(load_bias + ph->p_vaddr) + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);

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
    uint64_t load_bias = elf_compute_load_bias(&header, program_headers_start);

    if (info)
        memset(info, 0, sizeof(*info));

    for (uint16_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr* prog_header = (Elf64_Phdr*)((uint8_t*)program_headers_start + ((uint64_t)i * header.e_phentsize));
        if (info)
            elf_record_tls_segment(prog_header, info);
        if (elf_map_program_header(prog_header, file_base_address, file_size, load_bias) != 0) {
            return NULL;
        }
    }

    if (info) {
        info->entry = load_bias + header.e_entry;
        info->phdr_addr = elf_stage_phdrs_for_user(program_headers_start,
                                                   (uint64_t)header.e_phnum * header.e_phentsize);
        if (info->phdr_addr == 0)
            info->phdr_addr = elf_runtime_addr_for_offset(program_headers_start, header.e_phnum, header.e_phoff, load_bias);
        info->phentsize = header.e_phentsize;
        info->phnum = header.e_phnum;
        if (elf_load_tls_template_from_memory(file_base_address, file_size, info) != 0)
            return NULL;
    }

    if (elf_apply_runtime_relocations(load_bias, program_headers_start, header.e_phnum) != 0)
        return NULL;

    return (void*)(load_bias + header.e_entry);
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

    if (elf_vfs_read_exact_path(path, (uint32_t)header.e_phoff, program_headers, (uint32_t)phdr_bytes) != 0) {
        eprintf("elf: failed to read program headers");
        kfree(program_headers);
        vfs_close(&file);
        return NULL;
    }

    vfs_close(&file);

    uint64_t load_bias = elf_compute_load_bias(&header, program_headers);

    if (info)
        memset(info, 0, sizeof(*info));

    for (uint16_t i = 0; i < header.e_phnum; ++i) {
        elf_log_load_progress(i, header.e_phnum, &program_headers[i]);
        if (info)
            elf_record_tls_segment(&program_headers[i], info);
        if (elf_map_program_header_from_vfs(&program_headers[i], path, size, i, load_bias) != 0) {
            kfree(program_headers);
            return NULL;
        }
    }

    if (info) {
        info->entry = load_bias + header.e_entry;
        info->phdr_addr = elf_stage_phdrs_for_user(program_headers, phdr_bytes);
        if (info->phdr_addr == 0)
            info->phdr_addr = elf_runtime_addr_for_offset(program_headers, header.e_phnum, header.e_phoff, load_bias);
        info->phentsize = header.e_phentsize;
        info->phnum = header.e_phnum;
        if (elf_load_tls_template_from_vfs(path, info) != 0) {
            kfree(program_headers);
            return NULL;
        }
        if (info->tls_memsz) {
            printf("elf: tls off=%x filesz=%u memsz=%u align=%u",
                   info->tls_offset,
                   info->tls_filesz,
                   info->tls_memsz,
                   info->tls_align);
        }
    }

    if (elf_apply_runtime_relocations(load_bias, program_headers, header.e_phnum) != 0) {
        kfree(program_headers);
        return NULL;
    }

    kfree(program_headers);
    void* entry = (void*)(load_bias + header.e_entry);
    return entry;
}

void* elf_load_from_vfs(const char* path)
{
    return elf_load_from_vfs_ex(path, NULL);
}
