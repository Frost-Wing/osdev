/**
 * @file kernel.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The basic requirement from kernel to other parts of the code.
 * @version 0.1
 * @date 2023-10-23
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <flanterm/flanterm.h>
#include <fb.h>
#include <hal.h>
#include <sse.h>
#include <acpi.h>
#include <graphics.h>
#include <opengl/glcontext.h>
#include <opengl/glbackend.h>
#include <stddef.h>
#include <limine.h>
#include <memory.h>
#include <strings.h>

extern uint64_t* kend;

void hcf();
void clear_interrupts();
void high_level_halt();

extern int terminal_rows;
extern int terminal_columns;

extern uint64_t* back_buffer;
extern uint64_t* front_buffer;