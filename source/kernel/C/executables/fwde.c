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
    char interrupt_opcode = 0xCD;
    if(signature[0] == interrupt_opcode &&
       signature[1] == '1' &&
       signature[2] == 'F' &&
       signature[3] == 'W' &&
       signature[4] == 'D' &&
       signature[5] == 'E'){
        return true;
    }
    else
        return false;
}

void process_IFL(InterruptFrame* frame){ // process Invalid FWDE Loading
    meltdown_screen("Frost Wing Deployed executable was not executed the way it should have!", __FILE__, __LINE__, 0xbadf1e, 0x0, 0xFfe);
    hcf2();
}

void execute_fwde(int64* addr, kernel_data* data){
    info("Verifying the FrostWing deployed executable!", __FILE__);

    int8* local = (int8*)addr;

    fwde_header* header = (fwde_header*)local;

    if(verify_signature(header->signature)){
        info("Signature is correct!", __FILE__);
    }else{
        error("Wrong signature! Abort.", __FILE__);
        return;
    }

    if(*(&(header->endian)+1) != 1 && *(&(header->endian)+1) != 2){
        error("Undetermined endian executable! Abort.", __FILE__);
        printf("Endian value = 0x%x", header->endian);
        return;
    }

    if(header->architecture == 1){ // 64 bits
        local += sizeof(fwde_header);
        info("Starting executing the FrostWing deployed executable...", __FILE__);

        entry_function execute_binary = (entry_function)local;

        info("Function is ready! executing it..", __FILE__);
        print("=========================================================\n");
        execute_binary(data);
        print("=========================================================\n");
    }else{
        error("Unsupported architecture!", __FILE__);
        return;
    }


    done("Completed successfully!", __FILE__);
}