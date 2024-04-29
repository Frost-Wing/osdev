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

void execute_elf(int64* address) {
    elf_x64_header *header = (elf_x64_header*)address;
    
    if (header->magic != elf_magic) {
        error("Not a valid ELF file to load!", __FILE__);
        return;
    }

    void (*entry_point)() = (void (*)(void)) (int64*)&header->entry;

    entry_point();
}