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
#include <cc-asm.h>

void init_hashing(void){
    cstring data = "PradoshGame";
    volatile uint64_t hash = hash_string(data);

                                //i can be some1's 👇
    volatile int eq = (((uint32_t)(hash >> 32) == 0xbf) && ((uint32_t)hash == (uint32_t)0x393e7fd));

    if(eq){
        done("Hashing worked as intended!", __FILE__);
    }else{
        error("Hashing failed!", __FILE__);
        debug_printf("high=%x low=%x\n",
                (uint32_t)(hash >> 32),
                (uint32_t)hash);

        debug_printf("exp high=%x low=%x\n",
                0xbfU,
                0x393e7fdU);

        hcf2();
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