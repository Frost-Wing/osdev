/**
 * @file sh_util.h
 * @author Pradosh (pradoshgame@gmail.com) & GAMINGNOOBdev(https://github.com/GAMINGNOOBdev)
 * @brief Utils for fw shell.
 * @version 0.1
 * @date 2025-01-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <basics.h>
#include <memory2.h>
#include <graphics.h>

#define BUFFER_SIZE 1024

typedef struct command_list_entry
{
    struct command_list_entry* prev;
    char* command;
    size_t length;
    struct command_list_entry* next;
} command_list_entry;

typedef struct
{
    command_list_entry* start;
    command_list_entry* end;
    size_t count;
} command_list;

/**
 * @brief Initialize the command list
 * 
 * @param lst Pointer to command list object
 */
void init_command_list(command_list* lst);

/**
 * @brief Dispose the command list
 * 
 * @param lst Pointer to command list object
 */
void dispose_command_list(command_list* lst);

/**
 * @brief Push a value to the command list
 * 
 * @param lst Pointer to command list object
 * @param value Command string (will be copied to a new pointer)
 */
void push_command_to_list(command_list* lst, const char* value);
