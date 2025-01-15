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
#include <stddef.h>
#include <strings2.h>
#include <heap.h>
#include <stdint.h>
#include <flanterm/flanterm.h>


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
    
    display_time();
}

void execute(const char* buffer, int argc, char** argv);

extern struct flanterm_context* ft_ctx;

int shell_main(int argc, char** argv){
    running = true;
    char* command = malloc(BUFFER_SIZE);
    size_t commandBufferSize = BUFFER_SIZE;
    size_t commandSize = 0;
    size_t cursor = 0;

    print("\x1b[2J\x1b[H");
    welcome_message();

    command_list commandHistory;
    init_command_list(&commandHistory);
    
    putc('\n');
    print(argv[0]);
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
            execute(command, argc, argv);
            // push_command_to_list(&commandHistory, command, cursor); // <-- doesn't work because of malloc i assume
            cursor = 0;
            commandSize = 0;
            memset(command, 0, commandBufferSize);

            if(running){
                print(argv[0]);
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
        print(green_color);
        print(argv[0]);
        print(reset_color);
        putc('\n');
    } else if(strcmp(buffer, "fwfetch") == 0){
        fwfetch();
        putc('\n');
    } else if (strncmp(buffer, "echo ", 5) == 0) { 
        char* arg = buffer + 5;
        if(arg[0] == '\"' && arg[strlen_(arg)-1] == '\''){
            arg++;
            arg[strlen_(arg)-1] = '\0';
        }else{
            printf("%s", arg);
        }
    } else if (strncmp(buffer, "user ", 5) == 0) { 
        // int num_tokens;
        // char** tokens = splitf(buffer, ' ', &num_tokens);

        // if (tokens != NULL) {
        //     user_main(num_tokens, tokens);
        // } else {
        //     error("Failed to split string.", __FILE__);
        // }
        
    } else {
        printf("fsh: %s: not found", buffer);
    }
}