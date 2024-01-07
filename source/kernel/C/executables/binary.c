/**
 * @file binary.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-01-07
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */
#include <executables/binary.h>

void execute_bin(int64* addr){
    info("Started executing the bin!", __FILE__);
    void (*execute_binary)() = (void (*)())addr;
    execute_binary();
    
    done("Completed successfully!", __FILE__);
}