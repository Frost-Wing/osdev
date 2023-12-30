/**
 * @file tarball.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-12-30
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <archive/tarball.h>

char* filenames[2 KiB];
char* file_datas[2 KiB];

int counter;

void extract_tarball(int64* tarball_addr) {
    uintptr_t addr = (uintptr_t)tarball_addr;

    while (1) {
        if(counter >= 2 KiB){
            error("You reached number of file limits of 2 KiB!", __FILE__);
            break;
        }
        struct tarball_header *header = (struct tar_header *)addr;

        if (header->name[0] == '\0') {
            break;
        }

        unsigned int size = strtol(header->size, NULL, 8);

        char *filename = header->name;
        char *contents = (char *)(addr + block_size);

        filenames[counter] = filename;
        file_datas[counter] = file_datas;

        addr += block_size + ((size + block_size - 1) / block_size) * block_size;
        counter++;
    }
}