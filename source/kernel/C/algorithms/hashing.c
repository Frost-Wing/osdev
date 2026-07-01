/**
 * @file hashing.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Contains source code for hashing and encrypting
 * @version 0.1
 * @date 2023-12-20
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <algorithms/hashing.h>
#include <debugger.h>
#include <stdint.h>

void init_hashing(void){
    cstring data = "PradoshGame";
    uint64_t hash = hash_string(data);

    if(hash == (uint64_t)0x393e7fd){
        done("Hashing worked as intended!", __FILE__);
    }else{
        error("Hashing failed!", __FILE__);
        debug_printf("Hash value = 0x%x (int:%d)", hash, hash);
    }
}

uint64_t hash_string(cstring data){
    uint64_t hash = 0;
    while(*data){
        hash += (uint8_t)*data++;
        if(*data)
        hash *= (uint8_t)*data++;
    }
    return hash;
}

uint64 baranium_hash(const char* name)
{
    uint64 identifier = 9780;
    size_t length = (size_t)strlen(name);
    for (size_t i = 0; i < length; i++)
    {
        uint8_t c = (uint8_t)name[i];
        identifier = ((identifier << 5) + identifier) + c;
    }
    return (uint64)identifier;
}