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

#include <basics.h>
#include <sh_util.h>
#include <memory2.h>
#include <keyboard.h>
#include <heap.h>
#include <stddef.h>
#include <strings2.h>
#include <heap.h>
#include <stdint.h>
#include <flanterm/flanterm.h>
#include <filesystems/fwrfs.h>
#include <fdlfcn.h>
#include <heap.h>

void init_command_list(command_list* lst)
{
    if (lst == NULL)
        return;

    memset(lst, 0, sizeof(command_list));
}

void dispose_command_list(command_list* lst)
{
    if (lst == NULL)
        return;

    for (command_list_entry* entry = lst->start; entry != NULL;)
    {
        command_list_entry* next = entry->next;
        free(entry->command);
        free(entry);
        entry = next;
    }
}

void push_command_to_list(command_list* lst, const char* value, size_t length)
{
    if (lst == NULL || value == NULL || length == 0)
        return;

    command_list_entry* entry = malloc(sizeof(command_list_entry));
    assert(entry != NULL, __FILE__, __LINE__);
    if (entry == NULL)
        return;

    memset(entry, 0, sizeof(command_list_entry));
    entry->length = length;
    entry->command = malloc(length+1);
    memset(entry->command, 0, length+1);
    memcpy(entry->command, value, length);

    if (lst->start == NULL)
    {
        lst->start = lst->end = entry;
        lst->count++;
        return;
    }

    entry->prev = lst->end;
    lst->end->next = entry;
    lst->end = entry;
    lst->count++;
}

bool running = true;

void welcome_message(){
    print("\e[1;34m ______             _           _  _____ _          _ _ \n");
    printf("|  ____|           | |         | |/ ____| |        | | |");
    printf("| |__ _ __ ___  ___| |_ ___  __| | (___ | |__   ___| | |");
    printf("|  __| '__/ _ \\/ __| __/ _ \\/ _` |\\___ \\| '_ \\ / _ \\ | |");
    printf("| |  | | | (_) \\__ \\ ||  __/ (_| |____) | | | |  __/ | |");
    print("|_|  |_|  \\___/|___/\\__\\___|\\__,_|_____/|_| |_|\\___|_|_|\e[0m\n\n");

    print("\033[1;32mWelcome to frosted shell!\033[0m This is an implementation of \033[0;34msh\033[0m.\n");
    print("We as the developers try to make this shell as similar as \033[0;34msh\033[0m.\n\n");

    print("Website : \e[1;34mhttps://prad.digital\033[0m\n");
    print("Wiki    : \e[1;34mhttps://github.com/Frost-Wing/osdev/wiki\033[0m\n");
    print("Github  : \e[1;34mhttps://github.com/Frost-Wing\033[0m\n\n");

    print("Run \e[1;34mfrostedwm\033[0m to execute the window-manager.\n\n");
    
    display_time();
}

extern int64* wm_addr;

void start_window_manager(){
    void* file_addr = wm_addr;
    fdlfcn_handle* handle = fdlopen(file_addr, FDL_IMMEDIATE);
    int(*startfunction)(void);
    startfunction = (int(*)(void))fdlsym(FLD_NEXT, "_start");
    if (startfunction != NULL)
    {
        int result = startfunction();
        printf("Result function: %d\n", result);
        info("Successfully loaded function from .so file", __FILE__);
    }
    fdlclose(handle);
}

extern struct flanterm_context* ft_ctx;

struct fwrfs* fs;

int shell_main(int argc, char** argv){
    running = true;
    char* command = malloc(BUFFER_SIZE);
    size_t commandBufferSize = BUFFER_SIZE;
    size_t commandSize = 0;
    size_t cursor = 0;

    fs = (struct fwrfs*)malloc(sizeof(struct fwrfs));
    fs->nfiles = 0; // RAM Filesystem, so no need to permananly store files.
    fs->nfolders = 0;

    print("\x1b[2J\x1b[H");
    welcome_message();

    command_list commandHistory;
    init_command_list(&commandHistory);
    
    putc('\n');

    if(strcmp(argv[0], "root") == 0){
        print(red_color);
        print(argv[0]);
        print(reset_color);
    } else {
        print(green_color);
        print(argv[0]);
        print(reset_color);
    }
        
    print(" % ");

    uint8_t commandPulledFromHistory = 0;
    command_list_entry* entry = NULL;
    char c;
    while (running)
    {
        c = getc();

        if (c == 0)
            continue;

        if (c == '\n')
        {
            command[cursor] = '\0'; // Null-terminate the string
            putc('\n');
            if(strncmp(command, "sudo ", 5) == 0){
                int status = ask_password(argv[0]);
                
                if(status == 0){
                    argv[1] = (char*)1;
                    execute(command+5, argc, argv);
                } else {
                    error("Invalid password passed!", __FILE__);
                }
            } else {
                execute(command, argc, argv);
            }
            // push_command_to_list(&commandHistory, command, cursor); // <-- doesn't work because of malloc i assume
            cursor = 0;
            commandSize = 0;
            memset(command, 0, commandBufferSize);

            if(running){
                if(strcmp(argv[0], "root") == 0){
                    print(red_color);
                    print(argv[0]);
                    print(reset_color);
                } else {
                    print(green_color);
                    print(argv[0]);
                    print(reset_color);
                }
                print(" % ");
            }
            continue;
        }
        else if (c == '\b')
        {
            if (cursor > 0)
                cursor--;
            else continue;
        }
        else
        {
            if (commandSize+1 >= commandBufferSize)
            {
                commandBufferSize += BUFFER_SIZE;
                command = realloc(command, commandBufferSize);
            }

            command[cursor] = (char)c;
            cursor++;
            commandSize++;
        }

        putc(c);
    }
    free(command);

    dispose_command_list(&commandHistory);

    return 0;
}

void user_main(char* buffer){
    char tokens[MAX_WORDS][MAX_WORD_LEN];
    int num_tokens = 0;

    split(buffer, tokens, &num_tokens);

    if(num_tokens == 1){
        error("Please specify arguments!", __FILE__);
        return;
    }

    if(strcmp( leading_trailing_trim(tokens[1]), "add") == 0){
        if(num_tokens <= 2){
            error("You need to specify the password also!", __FILE__);
            return;
        }
        create_user(tokens[2], tokens[3]);
    }
}

void execute(const char* buffer, int argc, char** argv)
{
    if (buffer == NULL)
        return;
    if (strlen_(buffer) == 0)
        return;

    if(strcmp(buffer, "exit") == 0){
        print("\x1b[2J\x1b[H");
        running = false;
    } else if(strcmp(buffer, "clear") == 0){
        print("\x1b[2J\x1b[H");
    } else if(strcmp(buffer, "shutdown") == 0){
        info("Goodbye from Frosted Shell...", __FILE__);
        shutdown();
    } else if(strcmp(buffer, "reboot") == 0){
        info("Goodbye from Frosted Shell...", __FILE__);
        acpi_reboot();
    } else if(strcmp(buffer, "whoami") == 0){
        if(strcmp(argv[0], "root") == 0){
            print(red_color);
            print(argv[0]);
            print(reset_color);
        } else {
            print(green_color);
            print(argv[0]);
            print(reset_color);
        }
        putc('\n');
    } else if(strcmp(buffer, "fwfetch") == 0){
        fwfetch();
        putc('\n');
    } else if (strncmp(buffer, "echo ", 5) == 0) { 
        char* arg = buffer + 5;

        if(contains(arg, ">")){
            char words[MAX_WORDS][MAX_WORD_LEN];
            int num = 0;

            splitw(arg, words, &num, '>');

            write_file(fs, leading_trailing_trim(words[1]), leading_trailing_trim(words[0]));
        }
        else
            printf("%s", arg);
        
    } else if (strcmp(buffer, "echo") == 0) { 
        // do nothing.
    } else if (strncmp(buffer, "touch ", 6) == 0) { 
        char* arg = buffer + 6;
        create_file(fs, arg, "");
    } else if (strcmp(buffer, "touch") == 0) { 
        printf("touch: missing file operand");
    } else if (strncmp(buffer, "rm ", 3) == 0) { 
        char* arg = buffer + 3;
        delete_file(fs, arg);
    } else if (strcmp(buffer, "rm") == 0) { 
        printf("rm: missing file operand");
    } else if (strncmp(buffer, "mkdir ", 6) == 0) { 
        char* arg = buffer + 6;
        create_folder(fs, arg);
    } else if (strcmp(buffer, "mkdir") == 0) { 
        printf("mkdir: missing file operand");
    } else if (strncmp(buffer, "cat ", 4) == 0) { 
        char* arg = buffer + 4;
        printf("%s", read_file(fs, arg));
    } else if (strcmp(buffer, "touch") == 0) { 
        printf("touch: missing file operand");
    }  else if (strcmp(buffer, "ls") == 0) { 
        list_contents(fs);
    } else if (strncmp(buffer, "frostedwm", 9) == 0 || strcmp(buffer, "frostedwm") == 0) { 
        start_window_manager();
    } else if (strncmp(buffer, "meminfo", 7) == 0 || strcmp(buffer, "meminfo") == 0) { 
        display_memory_formatted(memory);
    } else if (strncmp(buffer, "heapinfo", 7) == 0 || strcmp(buffer, "heapinfo") == 0) { 
        mm_print_out();
    } else if (strncmp(buffer, "user ", 5) == 0 || strcmp(buffer, "user") == 0) {  
        if(argv[1] != null)
            if((int)argv[1] == 1)
                user_main(buffer);
            else
                printf("Must be a root user or have permissions to execute this command. (2)");
        else
            printf("Must be a root user or have permissions to execute this command. (1)");
    } else {
        printf("fsh: %s: not found", buffer);
    }
}