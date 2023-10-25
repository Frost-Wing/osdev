#include <stdint.h>
#include <graphics.h>
#include <kernel.h>
#include <opengl/glbackend.h>

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
    printf("Error Message : %s", message);
    printf("Error Code    : %d\n", error_code);
    print("=[ Handler Information ]=\n\t");
    printf("File name   : %s", file);
    printf("\tLine number : %d", line);
}