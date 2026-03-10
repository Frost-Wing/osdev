/**
 * @file exec.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Executes an ELF from VFS under userland
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>
#include <executables/elf.h>

int cmd_exec(int argc, char** argv)
{
    if (argc < 2) {
        printf("exec: missing program");
        printf("usage: exec <program> [args]");
        return 1;
    }

    const char* path = argv[1];

    void* entry = elf_load_from_vfs(path);

    if(!entry)
    {
        printf("exec: failed to load ELF");
        return -1;
    }
    
    void (*program)() = entry;
    program();
}