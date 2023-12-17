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

void meltdown_screen(cstring message, cstring file, int line, int64 error_code, int64 cr2, int64 int_no){
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

    printf("Timestamp     : %d:%d:%d %d/%d/%d", hour, minute, second, month, day, year);
    print("Error Message : \"");
    print(message);
    print("\"\n");
    print("Error Code    : ");
    printf("0x%x", error_code);
    printf("CR2           : 0x%x", cr2);
    printf("Interrupt No. : 0x%x", int_no);
    print("\n");
    print("=[ Handler Information ]=\n\t");
    print("File name   : ");
    print(file);
    print("\n");
    print("\tLine number : ");
    printf("%d", line);

    registers_dump();
    print("\n");
    frost_compilation_information();
    
    flush_heap();
}