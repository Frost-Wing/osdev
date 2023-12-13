/**
 * @file meltdown.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The code to print the Meltdown (Panic) message
 * @version 0.1
 * @date 2023-10-29
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <meltdown.h>

/**
 * @brief The Meltdown (Panic) Screen
 * 
 * @param message The Reason to cause a panic
 * @param file Handler's file
 * @param line Handler's line
 * @param error_code Error codes from registers
 */
void meltdown_screen(const char * message, const char* file, int line, uint64_t error_code){
    print("\x1b[2J");
    print("\x1b[H");

    print("\x1b[41m");
    for(int x=0;x<=terminal_columns*terminal_rows;x++){
        print(" ");
    }
    print("\x1b[H");
    print("===[ Meltdown Occurred at Wing Kernel! ]===\n\n");

    uint8_t second, minute, hour, day, month, year;
    update_system_time(&second, &minute, &hour, &day, &month, &year);

    printf("Timestamp     : %d:%d:%d %d/%d/%d\n", hour, minute, second, month, day, year);
    print("Error Message : \"");
    print(message);
    print("\"\n");
    print("Error Code    : ");
    printf("%d", error_code);
    print("\n");
    print("=[ Handler Information ]=\n\t");
    print("File name   : ");
    print(file);
    print("\n");
    print("\tLine number : ");
    printf("%d", line);
}