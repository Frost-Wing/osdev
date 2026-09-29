/**
 * @file elf.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-01-02
 *
 * @copyright Copyright (c) Pradosh 2024-2026
 *
 */
#include <debugger.h>
#include <executables/elf.h>
#include <fdlfcn.h>
#include <filesystems/vfs.h>
#include <graphics.h>
#include <heap.h>
#include <memory.h>
#include <paging.h>
#include <stdint.h>
#include <strings.h>
#include <userland.h>

#define ELF_USER_VADDR_MIN 0x1000ULL
#define ELF_USER_VADDR_MAX 0x0000800000000000ULL

#define ELF_MAX_IMAGE_SIZE (1ULL << 30)
#define ELF_MAX_PHNUM 128
#define ELF_PAGE_MASK (~0xFFFULL)
#define ELF_GLIBC_INTERP "/lib64/ld-linux-x86-64.so.2"
#define ELF_MUSL_INTERP "/lib/ld-musl-x86_64.so.1"

typedef struct {
    int (*read)(void *ctx, uint64_t off, void *dst, uint64_t len);
    void *ctx;
    uint64_t size;
} elf_src_t;

typedef struct {
    uint64_t mapped_end;
    uint64_t last_phys;
    uint64_t last_flags;
} elf_map_state_t;

/* ---------------------------------------------------------------- VFS ---- */

static uint32_t *elf_vfs_pos_ptr(vfs_file_t *file) {
    if (!file || !file->mnt)
        return NULL;

    switch (file->mnt->type) {
        case FS_FAT16:
            return &file->f.fat16.pos;
        case FS_FAT32:
            return &file->f.fat32.pos;
        case FS_ISO9660:
            return &file->f.iso9660.pos;
        case FS_EXT2:
            return &file->f.ext2.pos;
        case FS_UNKNOWN:
        case FS_FAT12:
        case FS_EXFAT:
        case FS_EXT3:
        case FS_EXT4:
        case FS_XFS:
        case FS_BTRFS:
        case FS_NTFS:
        case FS_UDF:
        case FS_PROC:
        case FS_DEV:
        default:
            return NULL;
    }
}

static uint64_t elf_vfs_file_size(vfs_file_t *file) {
    switch (file->mnt->type) {
        case FS_FAT16:
            return file->f.fat16.entry.filesize;
        case FS_FAT32:
            return file->f.fat32.entry.file_size;
        case FS_ISO9660:
            return file->f.iso9660.entry.size;
        case FS_EXT2:
            return ((uint64_t)file->f.ext2.inode.i_size_high << 32) | file->f.ext2.inode.i_size;
        case FS_UNKNOWN:
        case FS_FAT12:
        case FS_EXFAT:
        case FS_EXT3:
        case FS_EXT4:
        case FS_XFS:
        case FS_BTRFS:
        case FS_NTFS:
        case FS_UDF:
        case FS_PROC:
        case FS_DEV:
        default:
            return 0;
    }
}

static int elf_vfs_seek(vfs_file_t *file, uint32_t offset) {
    uint32_t *pos = elf_vfs_pos_ptr(file);
    if (!pos)
        return -1;

    if (*pos == offset)
        return 0;

    if (file->mnt->type == FS_FAT32) {
        fat32_file_t *fat32 = &file->f.fat32;
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

static int elf_vfs_read_exact(vfs_file_t *file, uint32_t offset, void *buf, uint32_t size) {
    if (elf_vfs_seek(file, offset) != 0)
        return -1;

    int rd = vfs_read(file, (uint8_t *)buf, size);
    return (rd >= 0 && (uint32_t)rd == size) ? 0 : -1;
}

/* ------------------------------------------------------------- sources --- */

static int elf_mem_read(void *ctx, uint64_t off, void *dst, uint64_t len) {
    memcpy(dst, (const uint8_t *)ctx + off, len);
    return 0;
}

static int elf_vfs_src_read(void *ctx, uint64_t off, void *dst, uint64_t len) {
    if (off > UINT32_MAX || len > UINT32_MAX - off)
        return -1;
    return elf_vfs_read_exact((vfs_file_t *)ctx, (uint32_t)off, dst, (uint32_t)len);
}

static int elf_src_read(const elf_src_t *src, uint64_t off, void *dst, uint64_t len) {
    if (off > src->size || len > src->size - off)
        return -1;
    if (len == 0)
        return 0;
    return src->read(src->ctx, off, dst, len);
}

static bool elf_in_range(uint64_t lo, uint64_t hi, uint64_t addr, uint64_t len) {
    return addr >= lo && addr <= hi && len <= hi - addr;
}

/* --------------------------------------------------------- relocations --- */

static int elf_symbol_value(const Elf64_Sym *symtab, uint64_t sym_count, uint32_t sym_index, uint64_t load_bias, uint64_t *out) {
    if (sym_index == 0) {
        *out = 0;
        return 0;
    }

    if (!symtab || sym_index >= sym_count)
        return -1;

    const Elf64_Sym *sym = &symtab[sym_index];
    if (sym->st_shndx == SHN_UNDEF) {
        if (ELF64_ST_BIND(sym->st_info) != STB_WEAK)
            return -1;
        *out = 0;
        return 0;
    }

    *out = (sym->st_shndx == SHN_ABS) ? sym->st_value : load_bias + sym->st_value;
    return 0;
}

static int elf_apply_rela_table(const Elf64_Rela *relocs,
    uint64_t reloc_count,
    const Elf64_Sym *symtab,
    uint64_t sym_count,
    uint64_t load_bias,
    uint64_t lo,
    uint64_t hi) {
    for (uint64_t i = 0; i < reloc_count; ++i) {
        const Elf64_Rela *reloc = &relocs[i];
        uint32_t type = ELF64_R_TYPE(reloc->r_info);
        uint32_t sym_index = ELF64_R_SYM(reloc->r_info);

        if (type == R_X86_64_NONE)
            continue;

        uint64_t target_addr = load_bias + reloc->r_offset;
        if (!elf_in_range(lo, hi, target_addr, sizeof(uint64_t))) {
            eprintf("elf: relocation target outside image off=%x", reloc->r_offset);
            return -1;
        }

        uint64_t *target = (uint64_t *)target_addr;
        uint64_t sym_value = 0;

        switch (type) {
            case R_X86_64_RELATIVE:
                *target = load_bias + (uint64_t)reloc->r_addend;
                break;

            case R_X86_64_64:
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
                if (elf_symbol_value(symtab, sym_count, sym_index, load_bias, &sym_value) != 0) {
                    eprintf("elf: unresolved symbol %u (reloc type=%u)", sym_index, type);
                    return -1;
                }
                *target = (type == R_X86_64_64) ? sym_value + (uint64_t)reloc->r_addend : sym_value;
                break;

            case R_X86_64_IRELATIVE:
                eprintf("elf: IRELATIVE unsupported (resolvers must not run in kernel mode)");
                return -1;

            default:
                eprintf("elf: unsupported reloc type=%u sym=%u off=%x add=%x",
                    type,
                    sym_index,
                    reloc->r_offset,
                    reloc->r_addend);
                return -1;
        }
    }

    return 0;
}

static int elf_apply_runtime_relocations(uint64_t load_bias, const Elf64_Phdr *phdrs, uint16_t phnum, uint64_t lo, uint64_t hi) {
    const Elf64_Phdr *dynph = NULL;
    for (uint16_t i = 0; i < phnum; ++i) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dynph = &phdrs[i];
            break;
        }
    }

    if (!dynph)
        return 0;

    uint64_t dyn_addr = load_bias + dynph->p_vaddr;
    uint64_t dyn_count = dynph->p_memsz / sizeof(Elf64_Dyn);
    if (dyn_count == 0)
        return 0;

    if (!elf_in_range(lo, hi, dyn_addr, dyn_count * sizeof(Elf64_Dyn))) {
        eprintf("elf: PT_DYNAMIC outside image");
        return -1;
    }

    const Elf64_Dyn *dynamic = (const Elf64_Dyn *)dyn_addr;

    uint64_t rela_ptr = 0, rela_sz = 0, rela_ent = sizeof(Elf64_Rela);
    uint64_t jmp_ptr = 0, jmp_sz = 0;
    uint64_t sym_ptr = 0, sym_ent = sizeof(Elf64_Sym);
    uint64_t hash_ptr = 0;

    for (uint64_t i = 0; i < dyn_count; ++i) {
        const Elf64_Dyn *dyn = &dynamic[i];
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
            case DT_JMPREL:
                jmp_ptr = dyn->d_un.d_ptr;
                break;
            case DT_PLTRELSZ:
                jmp_sz = dyn->d_un.d_val;
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

    if ((!rela_ptr || !rela_sz) && (!jmp_ptr || !jmp_sz))
        return 0;

    if (rela_ent != sizeof(Elf64_Rela)) {
        eprintf("elf: invalid DT_RELAENT");
        return -1;
    }
    if (sym_ent != sizeof(Elf64_Sym)) {
        eprintf("elf: invalid DT_SYMENT");
        return -1;
    }

    const Elf64_Sym *symtab = NULL;
    uint64_t sym_count = 0;
    if (sym_ptr && hash_ptr) {
        uint64_t hash_addr = load_bias + hash_ptr;
        uint64_t sym_addr = load_bias + sym_ptr;

        if (!elf_in_range(lo, hi, hash_addr, 2 * sizeof(uint32_t))) {
            eprintf("elf: DT_HASH outside image");
            return -1;
        }

        sym_count = ((const uint32_t *)hash_addr)[1];
        if (!elf_in_range(lo, hi, sym_addr, sym_count * sizeof(Elf64_Sym))) {
            eprintf("elf: DT_SYMTAB outside image");
            return -1;
        }
        symtab = (const Elf64_Sym *)sym_addr;
    }

    for (int t = 0; t < 2; ++t) {
        uint64_t ptr = t ? jmp_ptr : rela_ptr;
        uint64_t size = t ? jmp_sz : rela_sz;

        if (!ptr || !size)
            continue;

        if ((size % sizeof(Elf64_Rela)) != 0 || !elf_in_range(lo, hi, load_bias + ptr, size)) {
            eprintf("elf: invalid relocation table");
            return -1;
        }

        if (elf_apply_rela_table((const Elf64_Rela *)(load_bias + ptr),
                size / sizeof(Elf64_Rela),
                symtab,
                sym_count,
                load_bias,
                lo,
                hi) != 0)
            return -1;
    }

    return 0;
}

/* ------------------------------------------------------------- loading --- */

static int elf_validate_header(const Elf64_Ehdr *header, uint64_t file_size) {
    if (memcmp(&header->e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 || header->e_ident[EI_CLASS] != ELFCLASS64 ||
        header->e_ident[EI_DATA] != ELFDATA2LSB || (header->e_type != ET_EXEC && header->e_type != ET_DYN) ||
        header->e_machine != EM_X86_64 || header->e_version != EV_CURRENT) {
        error("Not a valid ELF file to load!", __FILE__);
        return -1;
    }

    if (header->e_phentsize != sizeof(Elf64_Phdr) || header->e_phnum == 0 || header->e_phnum > ELF_MAX_PHNUM ||
        header->e_phoff > file_size ||
        (uint64_t)header->e_phnum * header->e_phentsize > file_size - header->e_phoff) {
        eprintf("elf: invalid program header table");
        return -1;
    }

    return 0;
}

/* PT_LOAD segments must be sorted and non-overlapping (as the ELF spec requires). */
static int elf_scan_segments(const Elf64_Phdr *phdrs, uint16_t phnum, uint64_t file_size, uint64_t *out_lo, uint64_t *out_hi) {
    uint64_t lo = UINT64_MAX;
    uint64_t hi = 0;
    uint64_t prev_end = 0;
    uint64_t total = 0;

    for (uint16_t i = 0; i < phnum; ++i) {
        const Elf64_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD || ph->p_memsz == 0)
            continue;

        if (ph->p_memsz < ph->p_filesz || ph->p_memsz > ELF_MAX_IMAGE_SIZE ||
            ph->p_offset > file_size || ph->p_filesz > file_size - ph->p_offset ||
            ph->p_vaddr >= ELF_USER_VADDR_MAX) {
            eprintf("elf: invalid PT_LOAD bounds");
            return -1;
        }

        if (ph->p_vaddr < prev_end) {
            eprintf("elf: overlapping or unsorted PT_LOAD");
            return -1;
        }

        prev_end = ph->p_vaddr + ph->p_memsz;
        total += ph->p_memsz;
        if (total > ELF_MAX_IMAGE_SIZE) {
            eprintf("elf: image too large");
            return -1;
        }

        uint64_t start = ph->p_vaddr & ELF_PAGE_MASK;
        uint64_t end = (prev_end + 0xFFFULL) & ELF_PAGE_MASK;
        if (start < lo)
            lo = start;
        if (end > hi)
            hi = end;
    }

    if (lo == UINT64_MAX) {
        eprintf("elf: no loadable segments");
        return -1;
    }

    *out_lo = lo;
    *out_hi = hi;
    return 0;
}

static uint64_t elf_merge_flags(uint64_t a, uint64_t b) {
    uint64_t nx = a & b & (uint64_t)PAGE_NX;
    return ((a | b) & ~(uint64_t)PAGE_NX) | nx;
}

static int elf_map_segment(const Elf64_Phdr *ph, const elf_src_t *src, uint64_t load_bias, elf_map_state_t *st) {
    uint64_t vaddr = load_bias + ph->p_vaddr;
    uint64_t seg_start = vaddr & ELF_PAGE_MASK;
    uint64_t seg_end = (vaddr + ph->p_memsz + 0xFFFULL) & ELF_PAGE_MASK;

    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    if (ph->p_flags & PF_W)
        page_flags |= PAGE_RW;
    if (!(ph->p_flags & PF_X))
        page_flags |= PAGE_NX;

    /* Sorted segments can only share the last page of the previous one. */
    bool shared = seg_start < st->mapped_end;
    uint64_t first = seg_start;
    if (shared) {
        uint64_t merged = elf_merge_flags(st->last_flags, page_flags);
        if (merged != st->last_flags) {
            map_user_page(seg_start, st->last_phys, merged);
            st->last_flags = merged;
        }
        first = seg_start + PAGE_SIZE;
    }

    for (uint64_t page = first; page < seg_end; page += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        if (!phys) {
            eprintf("elf: out of memory mapping segment");
            return -1;
        }
        map_user_page(page, phys, page_flags);
        st->last_phys = phys;
        st->last_flags = page_flags;
    }
    st->mapped_end = seg_end;

    if (!shared)
        memset((void *)seg_start, 0, vaddr - seg_start);

    if (elf_src_read(src, ph->p_offset, (void *)vaddr, ph->p_filesz) != 0) {
        eprintf("elf: failed to read segment off=%x size=%u", ph->p_offset, ph->p_filesz);
        return -1;
    }

    memset((uint8_t *)vaddr + ph->p_filesz, 0, seg_end - (vaddr + ph->p_filesz));

    // printf("Loaded user segment -> vaddr=%x size=%u flags=%x", vaddr, ph->p_memsz, ph->p_flags);
    return 0;
}

static int elf_record_interp_segment(const Elf64_Phdr *ph, const elf_src_t *src, elf_image_info_t *info) {
    if (ph->p_filesz == 0 || ph->p_filesz >= sizeof(info->interp_path)) {
        eprintf("elf: invalid PT_INTERP bounds");
        return -1;
    }

    if (elf_src_read(src, ph->p_offset, info->interp_path, ph->p_filesz) != 0) {
        eprintf("elf: failed to read PT_INTERP");
        return -1;
    }

    info->interp_path[ph->p_filesz - 1] = '\0';
    info->has_interp = true;
    return 0;
}

static int elf_load_tls_template(const elf_src_t *src, elf_image_info_t *info) {
    if (info->tls_filesz == 0)
        return 0;

    if (info->tls_memsz < info->tls_filesz || info->tls_memsz > ELF_MAX_IMAGE_SIZE) {
        eprintf("elf: invalid PT_TLS bounds");
        return -1;
    }

    info->tls_template = kmalloc(info->tls_filesz);
    if (!info->tls_template) {
        eprintf("elf: failed to allocate TLS template");
        return -1;
    }

    if (elf_src_read(src, info->tls_offset, info->tls_template, info->tls_filesz) != 0) {
        eprintf("elf: failed to read TLS template");
        kfree(info->tls_template);
        info->tls_template = NULL;
        return -1;
    }

    return 0;
}

static uint64_t elf_stage_phdrs_for_user(const Elf64_Phdr *headers, uint64_t phdr_bytes) {
    uint64_t base = USER_PHDR_VADDR;
    uint64_t aligned = (phdr_bytes + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);

    for (uint64_t off = 0; off < aligned; off += PAGE_SIZE) {
        uint64_t phys = allocate_page();
        if (!phys)
            return 0;
        map_user_page(base + off, phys, USER_DATA_FLAGS);
    }

    memcpy((void *)base, headers, phdr_bytes);
    memset((uint8_t *)base + phdr_bytes, 0, aligned - phdr_bytes);
    return base;
}

static uint64_t elf_runtime_addr_for_offset(const Elf64_Phdr *headers, uint16_t phnum, uint64_t file_offset, uint64_t load_bias) {
    for (uint16_t i = 0; i < phnum; ++i) {
        const Elf64_Phdr *ph = &headers[i];
        if (ph->p_type != PT_LOAD || ph->p_filesz == 0)
            continue;

        if (file_offset >= ph->p_offset && file_offset - ph->p_offset < ph->p_filesz)
            return load_bias + ph->p_vaddr + (file_offset - ph->p_offset);
    }

    return 0;
}

/* Loads one image. Returns its entry point, or NULL (info->tls_template may still be set; see elf_finish). */
static void *elf_load_image(const elf_src_t *src, uint64_t dyn_base, bool is_interp, elf_image_info_t *info) {
    Elf64_Ehdr header;
    Elf64_Phdr *phdrs = NULL;
    elf_map_state_t st = {0};
    uint64_t lo = 0, hi = 0, bias = 0, rlo = 0, rhi = 0, phdr_bytes = 0;
    void *entry = NULL;

    memset(info, 0, sizeof(*info));

    if (elf_src_read(src, 0, &header, sizeof(header)) != 0) {
        eprintf("elf: failed to read header");
        return NULL;
    }

    if (elf_validate_header(&header, src->size) != 0)
        return NULL;

    phdr_bytes = (uint64_t)header.e_phnum * sizeof(Elf64_Phdr);
    phdrs = kmalloc(phdr_bytes);
    if (!phdrs) {
        eprintf("elf: failed to allocate program headers");
        return NULL;
    }

    if (elf_src_read(src, header.e_phoff, phdrs, phdr_bytes) != 0) {
        eprintf("elf: failed to read program headers");
        goto out;
    }

    if (elf_scan_segments(phdrs, header.e_phnum, src->size, &lo, &hi) != 0)
        goto out;

    bias = (header.e_type == ET_DYN) ? dyn_base - lo : 0;
    rlo = lo + bias;
    if (rlo < ELF_USER_VADDR_MIN || rlo >= ELF_USER_VADDR_MAX || hi - lo > ELF_USER_VADDR_MAX - rlo) {
        eprintf("elf: image outside user address range");
        goto out;
    }
    rhi = rlo + (hi - lo);

    for (uint16_t i = 0; i < header.e_phnum; ++i) {
        const Elf64_Phdr *ph = &phdrs[i];
        switch (ph->p_type) {
            case PT_LOAD:
                if (ph->p_memsz && elf_map_segment(ph, src, bias, &st) != 0)
                    goto out;
                break;
            case PT_TLS:
                info->tls_offset = ph->p_offset;
                info->tls_filesz = ph->p_filesz;
                info->tls_memsz = ph->p_memsz;
                info->tls_align = ph->p_align;
                break;
            case PT_INTERP:
                if (elf_record_interp_segment(ph, src, info) != 0)
                    goto out;
                break;
            default:
                break;
        }
    }

    info->entry = bias + header.e_entry;
    info->requested_entry = header.e_entry;
    info->load_bias = bias;
    info->phentsize = header.e_phentsize;
    info->phnum = header.e_phnum;

    if (!elf_in_range(rlo, rhi, info->entry, 1)) {
        eprintf("elf: entry point outside image");
        goto out;
    }

    if (!is_interp)
        info->phdr_addr = elf_stage_phdrs_for_user(phdrs, phdr_bytes);
    if (info->phdr_addr == 0)
        info->phdr_addr = elf_runtime_addr_for_offset(phdrs, header.e_phnum, header.e_phoff, bias);

    if (elf_load_tls_template(src, info) != 0)
        goto out;

    /* With an interpreter, ld.so relocates the program itself. */
    if (!info->has_interp && elf_apply_runtime_relocations(bias, phdrs, header.e_phnum, rlo, rhi) != 0)
        goto out;

    entry = (void *)info->entry;

out:
    kfree(phdrs);
    return entry;
}

static void *elf_load_interp(elf_image_info_t *info);

/* Loads the interpreter if needed and releases the TLS template on failure. */
static void *elf_finish(void *entry, bool is_interp, elf_image_info_t *info) {
    if (entry && info->has_interp) {
        if (is_interp) {
            eprintf("elf: nested interpreters are not supported");
            entry = NULL;
        } else {
            entry = elf_load_interp(info);
        }
    }

    if (!entry && info->tls_template) {
        kfree(info->tls_template);
        info->tls_template = NULL;
    }

    return entry;
}

static void *elf_load_image_vfs(vfs_file_t *file, uint64_t dyn_base, bool is_interp, elf_image_info_t *info) {
    elf_src_t src = {
        .read = elf_vfs_src_read,
        .ctx = file,
        .size = elf_vfs_file_size(file),
    };
    return elf_load_image(&src, dyn_base, is_interp, info);
}

static void *elf_load_interp(elf_image_info_t *info) {
    vfs_file_t file;
    elf_image_info_t interp_info;

    int rc = vfs_open(info->interp_path, VFS_RDONLY, &file);
    if (rc != 0 && strcmp(info->interp_path, ELF_GLIBC_INTERP) == 0) {
        snprintf(info->interp_path, sizeof(info->interp_path), "%s", ELF_MUSL_INTERP);
        rc = vfs_open(info->interp_path, VFS_RDONLY, &file);
    }

    if (rc != 0) {
        eprintf("elf: failed to load interpreter %s", info->interp_path);
        return NULL;
    }

    void *entry = elf_load_image_vfs(&file, USER_INTERP_VADDR, true, &interp_info);
    vfs_close(&file);
    entry = elf_finish(entry, true, &interp_info);

    if (!entry) {
        eprintf("elf: failed to load interpreter %s", info->interp_path);
        return NULL;
    }

    info->interp_base = interp_info.load_bias;
    if (interp_info.tls_template)
        kfree(interp_info.tls_template);

    return entry;
}

void *elf_load_from_memory_ex(void *file_base_address, uint64_t file_size, elf_image_info_t *info) {
    if (file_base_address == NULL)
        return NULL;

    elf_image_info_t local_info;
    elf_image_info_t *out = info ? info : &local_info;
    elf_src_t src = {
        .read = elf_mem_read,
        .ctx = file_base_address,
        .size = file_size,
    };

    void *entry = elf_finish(elf_load_image(&src, USER_CODE_VADDR, false, out), false, out);

    if (!info && out->tls_template)
        kfree(out->tls_template);

    return entry;
}

void *elf_load_from_memory(void *file_base_address, uint64_t file_size) {
    return elf_load_from_memory_ex(file_base_address, file_size, NULL);
}

void *elf_load_from_vfs_ex(const char *path, elf_image_info_t *info) {
    if (!path)
        return NULL;

    debug_printf("path = %s\n", path);

    vfs_file_t file;
    if (vfs_open(path, VFS_RDONLY, &file) != 0) {
        eprintf("elf: failed to open %s", path);
        return NULL;
    }

    elf_image_info_t local_info;
    elf_image_info_t *out = info ? info : &local_info;

    void *entry = elf_load_image_vfs(&file, USER_CODE_VADDR, false, out);
    vfs_close(&file);
    entry = elf_finish(entry, false, out);

    if (!info && out->tls_template)
        kfree(out->tls_template);

    return entry;
}

void *elf_load_from_vfs(const char *path) {
    return elf_load_from_vfs_ex(path, NULL);
}