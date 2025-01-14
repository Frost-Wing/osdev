/**
 * @file sh.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <sh_util.h>

bool running = true;

void welcome_message(){
    print("\e[0;34m ______             _           _  _____ _          _ _ \n");
    printf("|  ____|           | |         | |/ ____| |        | | |");
    printf("| |__ _ __ ___  ___| |_ ___  __| | (___ | |__   ___| | |");
    printf("|  __| '__/ _ \\/ __| __/ _ \\/ _` |\\___ \\| '_ \\ / _ \\ | |");
    printf("| |  | | | (_) \\__ \\ ||  __/ (_| |____) | | | |  __/ | |");
    print("|_|  |_|  \\___/|___/\\__\\___|\\__,_|_____/|_| |_|\\___|_|_|\e[0m\n\n");

    print("\033[1;32mWelcome to frosted shell!\033[0m This is an implementation of \033[1;34msh\033[0m.\n");
    print("We as the developers try to make this shell as similar as \033[1;34msh\033[0m.\n");
    
    display_time();
}

int shell_main(int argc, char** argv){
    char* buffer = (char*)malloc(BUFFER_SIZE * sizeof(char));
    int16 bufptr = 0;

    print("\x1b[2J\x1b[H");
    welcome_message();
    
    __putc('\n');
    print(argv[0]);
    __putc(' ');
    __putc('%');
    __putc(' ');
    int c;

    while (running) {
        c = getc(); 

        if (c == 0x1c) { // Enter key
            buffer[bufptr] = '\0'; // Null-terminate the string
            __putc('\n'); 
            execute(buffer);
            bufptr = 0; // Reset buffer pointer
            for (int i = 0; i < BUFFER_SIZE; i++) {
                buffer[i] = 0;
            }

            if(running){
                print(argv[0]);
                __putc(' ');
                __putc('%');
                __putc(' '); 
            }
        } else if (c == 0xe) { // Backspace key
            if (bufptr > 0) { 
                bufptr--; 
                buffer[bufptr] = '\0'; // Adjust null-terminator
                __putc('\b'); // Move cursor back
                __putc(' '); // Overwrite previous character
                __putc('\b'); // Move cursor back again
            }
        } else {
            if (bufptr < BUFFER_SIZE - 1) { 
                buffer[bufptr++] = (char)c;
            }
        }

        __putc(c); 
    }

    free(buffer);
    return 0;
}

void execute(const char* buffer){
    if(strcmp(buffer, "exit") == 0){
        print("\x1b[2J\x1b[H");
        running = false;
    } else if(strcmp(buffer, "shutdown") == 0){
        info("Goodbye from Frosted Shell...", __FILE__);
        shutdown();
    } else if(strcmp(buffer, "reboot") == 0){
        info("Goodbye from Frosted Shell...", __FILE__);
        acpi_reboot();
    } else {
        printf("fsh: :%s: not found (length : %d)", buffer, strlen_(buffer));
    }
}