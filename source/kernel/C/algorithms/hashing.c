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
    int64 hash = hash_string(data);

    int64 verification = (int64)0x393e7fd; // ! DO NOT use just the hex, use explicit type conversion, if not you will get a value while looks same but not equal.
    if(verification == 0x393e7fd){
        done("Hashing worked as intended!", __FILE__);
    }else{
        error("Hashing failed!", __FILE__);
        printf("Hash value = 0x%x (int:%d)", hash, hash);
    }
}

int64 hash_string(string data){
    int64 hash = 0;
    while(*data){
        hash += *data++;
        if(*data)
        hash *= *data++;
    }
    return hash;
}