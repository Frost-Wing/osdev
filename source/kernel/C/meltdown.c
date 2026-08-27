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
#include <graphics.h>
#include <crash_diagnostics.h>
#include <crash_symbols.h>
#include <isr.h> // For InterruptFrame
#include <meltdown.h>

#define clean_mode

static void meltdown_print_source(const CrashSymbolResult *symbol) {
    if (!symbol || !symbol->found) {
        printf("SOURCE LOCATION UNAVAILABLE");
        printf("    RIP = 0x%X", symbol ? symbol->rip : 0);
        printf("    No debug/source mapping exists for this address.");
        return;
    }

    printf("CRASH LOCATION");
    printf("    %s:%d", symbol->file, symbol->line);
    printf("    %s()", symbol->function);

    printf("SOURCE");
    if (!symbol->snippet || symbol->snippet_count == 0) {
        printf("    Source snippet unavailable for this address.");
        return;
    }

    for (uint32 i = 0; i < symbol->snippet_count; i++) {
        const CrashSourceLine *line = &symbol->snippet[i];
        if (line->line == symbol->line)
            printf(" >> %d | %s", line->line, line->text ? line->text : "");
        else
            printf("    %d | %s", line->line, line->text ? line->text : "");
    }
}

static void meltdown_print_diagnosis(cstring handler_file, int handler_line, const CrashSymbolResult *symbol, const CrashDiagnosis *diagnosis) {
    if (!diagnosis) {
        printf("[MELTDOWN] Crash diagnosis unavailable.");
        printf("RIP = 0x%X", symbol ? symbol->rip : 0);
        return;
    }

    printf("WHAT PROBABLY HAPPENED");
    printf("    %s", diagnosis->what_happened);

    printf("LIKELY CAUSE");
    printf("    %s", diagnosis->likely_cause);

    printf("WHAT TO FIX");
    printf("    %s", diagnosis->what_to_fix);
    if (symbol && symbol->found)
        printf("    Inspect: %s:%d", symbol->file, symbol->line);
    printf("    %s", diagnosis->source_hint);

    printf("CONFIDENCE: %s", crash_confidence_string(diagnosis->confidence));

    printf("PANIC HANDLER");
    printf("    %s:%d", handler_file, handler_line);
}

void meltdown_screen(cstring message, cstring file, int line, uint64 error_code, uint64 cr2, uint64 int_no, InterruptFrame *frame) {
#ifndef clean_mode
    print("\x1b[2J");
    print("\x1b[H");

    debug_println(message);

    print("\x1b[48;2;26;17;14m");
    for (int x = 0; x <= terminal_columns * terminal_rows; x++) {
        print(" ");
    }

    print("\x1b[H");
    print("===[ Meltdown Occurred at Wing Kernel! ]===\n\n");

    uint8_t second, minute, hour, day, month, year;
    update_system_time(&second, &minute, &hour, &day, &month, &year);

    printf("Timestamp     : %02d:%02d:%02d %02d/%02d/%02d", hour, minute, second, day, month, year);
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
#endif
#ifdef clean_mode
    uint64 rip = frame ? frame->rip : 0;
    CrashSymbolResult symbol;
    CrashDiagnosis diagnosis;

    crash_symbols_resolve(rip, &symbol);
    crash_diagnostics_analyze(int_no, error_code, cr2, frame, &symbol, &diagnosis);

    eprintf("[MELTDOWN] %s (int=%d err=0x%X cr2=0x%X rip=0x%X)", message, int_no, error_code, cr2, rip);
    meltdown_print_source(&symbol);
    meltdown_print_diagnosis(file, line, &symbol, &diagnosis);

    if (frame) {
        eprintf("[MELTDOWN] regs rax=0x%X rcx=0x%X rdx=0x%X rsi=0x%X rdi=0x%X r8=0x%X r9=0x%X r10=0x%X r11=0x%X",
            frame->rax,
            frame->rcx,
            frame->rdx,
            frame->rsi,
            frame->rdi,
            frame->r8,
            frame->r9,
            frame->r10,
            frame->r11);
        eprintf("[MELTDOWN] regs SS = 0x%X", frame->ss);
    }
#endif
}

void interrupt_frame_dump(InterruptFrame *frame) {
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
    printf("\tSS     = 0x%X", frame->ss);

    printf("===============================");
}
