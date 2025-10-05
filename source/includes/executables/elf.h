/**
 * @file elf.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-01-02
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */
#ifndef ELF_H
#define ELF_H
#include <basics.h>

// Basic ELF typedefs
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

// ELF identification indexes
#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_OSABI    7
#define EI_ABIVERSION 8
#define EI_PAD      9
#define EI_NIDENT   16

// ELF magic number
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFMAG  "\x7f""ELF"
#define SELFMAG 4

// ELF class
#define ELFCLASS64 2

// ELF data encoding
#define ELFDATA2LSB 1

// ELF type
#define ET_EXEC 2

// Machine type
#define EM_X86_64 62

// ELF version
#define EV_CURRENT 1

// Section types
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_NOBITS   8

// Special section indices
#define SHN_UNDEF    0

// Symbol binding
#define STB_LOCAL    0
#define STB_GLOBAL   1
#define STB_WEAK     2
#define ELF64_ST_BIND(info) ((info) >> 4)
#define ELF64_ST_TYPE(info) ((info) & 0xf)

// ELF file types
#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3

// x86_64 relocation types
#define R_X86_64_NONE       0
#define R_X86_64_64         1
#define R_X86_64_PC32       2
#define R_X86_64_GOT32      3
#define R_X86_64_PLT32      4
#define R_X86_64_COPY       5
#define R_X86_64_GLOB_DAT   6
#define R_X86_64_JUMP_SLOT  7
#define R_X86_64_RELATIVE   8
#define R_X86_64_GOTPCREL   9

// Extract symbol and type from r_info
#define ELF64_R_SYM(info)  ((info) >> 32)
#define ELF64_R_TYPE(info) ((uint32_t)(info))

// Program header
typedef struct {
    uint32_t p_type;   // Segment type
    uint32_t p_flags;  // Segment flags
    uint64_t p_offset; // Offset of segment in file
    uint64_t p_vaddr;  // Virtual address in memory
    uint64_t p_paddr;  // Physical address (ignored on x86_64)
    uint64_t p_filesz; // Size of segment in file
    uint64_t p_memsz;  // Size of segment in memory
    uint64_t p_align;  // Alignment of segment
} Elf64_Phdr;

typedef struct {
    unsigned char e_ident[16]; // Magic number and other info
    uint16_t      e_type;      // Object file type
    uint16_t      e_machine;   // Architecture
    uint32_t      e_version;   // Object file version
    uint64_t      e_entry;     // Entry point virtual address
    uint64_t      e_phoff;     // Program header table file offset
    uint64_t      e_shoff;     // Section header table file offset
    uint32_t      e_flags;     // Processor-specific flags
    uint16_t      e_ehsize;    // ELF header size in bytes
    uint16_t      e_phentsize; // Program header table entry size
    uint16_t      e_phnum;     // Program header table entry count
    uint16_t      e_shentsize; // Section header table entry size
    uint16_t      e_shnum;     // Section header table entry count
    uint16_t      e_shstrndx;  // Section header string table index
} Elf64_Ehdr;

// Section header
typedef struct {
    uint32_t sh_name;       // Section name (string tbl index)
    uint32_t sh_type;       // Section type
    uint64_t sh_flags;      // Section flags
    uint64_t sh_addr;       // Section virtual addr at execution
    uint64_t sh_offset;     // Section file offset
    uint64_t sh_size;       // Section size in bytes
    uint32_t sh_link;       // Link to another section
    uint32_t sh_info;       // Additional section information
    uint64_t sh_addralign;  // Section alignment
    uint64_t sh_entsize;    // Entry size if section holds table
} Elf64_Shdr;

// Symbol table entry
typedef struct {
    uint32_t st_name;  // Symbol name (string tbl index)
    unsigned char st_info;   // Symbol type and binding
    unsigned char st_other;  // Symbol visibility
    uint16_t st_shndx;       // Section index
    uint64_t st_value;       // Symbol value
    uint64_t st_size;        // Symbol size
} Elf64_Sym;

// Relocation entry with addend
typedef struct {
    uint64_t r_offset; // Address
    uint64_t r_info;   // Relocation type and symbol index
    int64_t  r_addend; // Addend
} Elf64_Rela;

void* elf_load_from_memory(void* file_base_address);
#endif