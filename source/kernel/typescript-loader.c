/**
 * @file typescript-loader.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief This file contains the pre-defines functions and variables for typescript.
 * @version 0.1
 * @date 2023-10-23
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <typescript-loader.h>

/**
 * @brief This function call a pre-compiled typescript code.
 * 
 */
void load_typescript(){
    info("Loading typescript...", __FILE__);
    int status = typescript_main();
    if(status == 0){
        done("Successfully loaded!", __FILE__);
    }else{
        error("Got return code as an non zero!", __FILE__);
    }
}