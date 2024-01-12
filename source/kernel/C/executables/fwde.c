/**
 * @file fwde.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The executor code for FrostWing Deployed Executable - 64 bits
 * @version 0.1
 * @date 2024-01-07
 * 
 * @copyright Copyright (c) Pradosh 2024
 * 
 */

#include <executables/fwde.h>

bool verify_signature(char* signature){
    if(signature[0] == 'F' &&
       signature[1] == 'W' &&
       signature[2] == 'D' &&
       signature[3] == 'E'){
        return true;
    }
    else
        return false;
}

void execute_fwde(int64* addr){
    info("Verifying the FrostWing deployed executable!", __FILE__);

    int8* local = (int8*)addr;

    fwde_header* header = (fwde_header*)local;

    if(verify_signature(header->signature)){
        info("Signature is correct!", __FILE__);
    }else{
        error("Wrong signature! Abort.", __FILE__);
        return;
    }

    if(header->endian != 1 && header->endian != 2){
        error("Undetermined endian executable! Abort.", __FILE__);
        return;
    }

    if(header->architecture == 1){ // 64 bits
        local += sizeof(fwde_header);
        if(*(local + header->raw_size) == 0){
            info("Valid executable! and ready for execution!", __FILE__);
        }
        info("Starting executing the FrostWing deployed executable...", __FILE__);
        int (*execute_binary)() = (int (*)())local;
        info("Function is ready!", __FILE__);
        int status_code = execute_binary();
        if(status_code != 0){
            error("Executable returned an non zero status code! (something went wrong on the executable side)", __FILE__);
            printf("Status code : 0x%x (%d)", status_code, status_code);
            return;
        }
    }else{
        error("Unsupported architecture!", __FILE__);
        return;
    }


    done("Completed successfully!", __FILE__);
}