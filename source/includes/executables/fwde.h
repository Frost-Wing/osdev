/**
 * @file fwde.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The executor header for FrostWing Deployed Executable - 64 bits
 * @version 0.1
 * @date 2024-01-07
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */
#include <basics.h>
#include <stdbool.h>

typedef struct {
    char signature[4]; // FWDE
    int8 architecture; // 1 = 64 bits; 2 = 32 bits; 3 = 16 bits; 4 = 8 bits
    int8 raw_size; // size of just the executable part and not the header
    int8 endian; // 0 = error; 1 = little; 2 = big
} fwde_header;

void execute_fwde(int64* addr);