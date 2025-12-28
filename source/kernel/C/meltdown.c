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
#include <isr.h> // For InterruptFrame

void meltdown_screen(cstring message, cstring file, int line, int64 error_code, int64 cr2, int64 int_no, InterruptFrame* frame){
    print("\x1b[2J");
    print("\x1b[H");

    debug_println(message);

    print("\x1b[48;2;26;17;14m");
    for(int x=0;x<=terminal_columns*terminal_rows;x++){
        print(" ");
    }

    print("\x1b[H");
    print("===[ Meltdown Occurred at Wing Kernel! ]===\n\n");

    uint8_t second, minute, hour, day, month, year;
    update_system_time(&second, &minute, &hour, &day, &month, &year);

    printf("Timestamp     : %d:%d:%d %d/%d/%d", hour, minute, second, day, month, year);
    printf("Error Message : %s", message);

    printf("Error Code    : 0x%X", error_code);

    printf("CR2           : 0x%X (%d)", cr2, cr2);
    printf("Interrupt No. : 0x%X (%d)", int_no, int_no);
    printf("Logged File   : %s", last_filename);

    print("\nLast instance of print being used :\n");
    printf("File          : %s:%d", last_print_file, last_print_line);
    printf("Function      : %s();", last_print_func);

    print("\n");

    printf("===[ Handler Information ]===");
    printf("\tFile name      : %s", file);
    printf("\tLine number    : %d", line);

    print("\n");

    interrupt_frame_dump(frame);

    print("\n");
    frost_compilation_information();
}

void interrupt_frame_dump(InterruptFrame* frame) {
    printf("===[ Interrupt Frame Dump ]===");

    printf(" General Purpose Registers:");
    printf("\tRAX = 0x%08X   RCX = 0x%X", frame->rax, frame->rcx);
    printf("\tRDX = 0x%08X   RSI = 0x%08X   RDI = 0x%X", frame->rdx, frame->rsi, frame->rdi);
    printf("\tR8  = 0x%08X   R9  = 0x%08X   R10 = 0x%08X   R11 = 0x%X", frame->r8, frame->r9, frame->r10, frame->r11);

    printf(" Control Registers:");
    printf("\tRIP    = 0x%X", frame->rip);
    printf("\tCS     = 0x%X", frame->cs);
    printf("\tRFLAGS = 0x%X", frame->rflags);
    printf("\tRSP    = 0x%X", frame->rsp);

    printf("===============================");
}