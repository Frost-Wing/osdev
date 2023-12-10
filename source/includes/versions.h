/**
 * @file versions.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief This is the header file which contains all the tools used's versions.
 * @version 0.1
 * @date 2023-12-10
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>
#include <graphics.h>

/**
 * @brief Contains GCC, CC, LD, MAKE, xorriso, tar versions.
 * 
 */
extern cstring versions;

/**
 * @brief Contains the exact time when the compilation started.
 * 
 */
extern cstring date;

static void frost_compilation_information(){
    print(yellow_color);
    print(versions);
    print(orange_color);
    printf("Compiled Time (Started at): %s", date);
    print(reset_color);
}