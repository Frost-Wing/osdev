/**
 * @file sh_util.h
 * @author Pradosh (pradoshgame@gmail.com) & GAMINGNOOBdev(https://github.com/GAMINGNOOBdev)
 * @brief Utils for fw shell.
 * @version 0.1
 * @date 2025-01-14
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#ifndef SH_UTIL_H
#define SH_UTIL_H

#include <basics.h>
#include <memory.h>
#include <graphics.h>
#include <memory.h>
#include <keyboard.h>
#include <heap.h>
#include <stddef.h>
#include <strings.h>
#include <stdint.h>
#include <flanterm/flanterm.h>
#include <filesystems/fwrfs.h>
#include <fdlfcn.h>
#include <commands/commands.h>

#define BUFFER_SIZE 128
#define MAX_COMMAND_LINE 1024
#define MAX_SUBCOMMANDS  64
#define MAX_ARGV         64

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
 * @brief Wrapper to store properly the function commands list.
 * 
 */
typedef int (*cmd_func_t)(int argc, char** argv);

/**
 * @brief Wrapper to store the command and its respective function.
 * 
 */
typedef struct {
    const char* name;
    cmd_func_t func;
} command_t;

/**
 * @brief operator types between commands 
 * 
 */
typedef enum {
    OP_NONE = 0,
    OP_AND,   /* && */
    OP_OR     /* || */
} op_t;

typedef struct {
    char* cmd;
    op_t op_after;
} subcmd_t;

/**
 * @brief Name of the current user.
 * 
 */
string current_user;

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
 * @param length Command string length
 */
void push_command_to_list(command_list* lst, const char* value, size_t length);

/**
 * @brief Executes the command passed to it.
 * 
 * @param buffer 
 * @param argc 
 * @param argv 
 */
void execute(const char* buffer, int argc, char** argv);

/**
 * @brief Function for adding or removing users.
 * 
 * @param argument_count 
 * @param argument_values 
 */
void user_main(char* buffer);

#endif