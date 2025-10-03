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

void init_hashing(){
    string data = "PradoshGame";
    int hash = hash_string(data);

    if(hash == 0x393e7fd){
        done("Hashing worked as intended!", __FILE__);
    }else{
        error("Hashing failed!", __FILE__);
        printf("Hash value = 0x%x (int:%d)", hash, hash);
    }
}

int64 hash_string(string data){
    int hash = 0;
    while(*data){
        hash += *data++;
        if(*data)
        hash *= *data++;
    }
    return hash;
}

int64 baranium_hash(const char* name)
{
    int64 identifier = 9780;
    size_t length = strlen_(name);
    for (size_t i = 0; i < length; i++)
    {
        char c = name[i];
        identifier = ((identifier << 5) + identifier) + c;
    }
    return (int64)identifier;
}