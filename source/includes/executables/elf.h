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
#include <basics.h>

#define elf_magic 0x464C457F // 0x7F, then 'ELF' in ASCII 

typedef struct {
    int32 magic;
    int8  _class;
    int8  data;
    int8  version;
    int8  osabi;
    int8  abiversion;
    int8  pad[7];
    int16 type;
    int16 machine;
    int32 version_elf;
    int64 entry;
    int64 ph_offset;
    int64 sh_offset;
    int32 flags;
    int16 header_size;
    int16 ph_entry_size;
    int16 ph_num;
    int16 sh_entry_size;
    int16 sh_num;
    int16 sh_str_index;
} elf_x64_header;

/**
 * @brief Unused yet!
 * 
 */
typedef struct {
    int32 type;
    int32 flags;
    int64 offset;
    int64 vaddr;
    int64 paddr;
    int64 filesz;
    int64 memsz;
    int64 align;
} elf_x64_program_header;

void execute_elf(int64* address);