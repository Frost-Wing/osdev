/**
 * @file user.c
 * @author your name (you@domain.com)
 * @brief Code for user code handling.
 * @version 0.1
 * @date 2025-01-15
 * 
 * @copyright Copyright (c) 2025
 * 
 */

void user_main(int argc, char** argv){

    if(argc <= 1){
        error("Please specify arguments!");
        return;
    }

    if (strcmp(argv[0], "add") == 0){
        info("Adding an user...", __FILE__);
    }
}