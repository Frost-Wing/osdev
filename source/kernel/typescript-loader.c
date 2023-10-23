#include <stdint.h>

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