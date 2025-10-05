/**
 * @file debugger.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief This code uses the E9 hack to make debugging a lot easier
 * @version 0.1
 * @date 2023-10-31
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <debugger.h>
#include <basics.h>
#include <stdarg.h>
#include <strings.h>

/**
 * @brief If there was possibility for the change in port, we can easily change the E9 Port.
 * 
 */
int port = 0xE9;

/**
 * @brief Puts a character to the address 0xE9 using x86_64 outb
 * 
 * @param c The single character that you want to print.
 */
void debug_putc(char c){
    #if debugger_mode
    outb(port, c);
    #endif
}

/**
 * @brief Puts a string to the address 0xE9 using debug_putc()
 * 
 * @param msg 
 */
void debug_print(cstring msg){
    while(*msg){
        debug_putc(*msg);
        msg++;
    }
}

/**
 * @brief Puts a string to the address 0xE9 using debug_putc() and prints a new line
 * 
 * @param msg 
 */
void debug_println(cstring msg){
    while(*msg){
        debug_putc(*msg);
        msg++;
    }
    debug_putc('\n');
}

/**
 * @brief printf implemented to debug.
 * 
 * @param format 
 * @param ... 
 */
void debug_printf(cstring format, ...)
{
    va_list argp;
    va_start(argp, format);

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            switch (*format)
            {
            case 'u':
                debug_print(uint_to_string(va_arg(argp, size_t)));
                break;

            case 'x':
                debug_print(hex_to_string(va_arg(argp, size_t), 0));
                break;
            
            case 's':
                debug_print(va_arg(argp, char*));
                break;

            case 'c':
                debug_putc(va_arg(argp, char));
                break;
            }
        } else {
            if(*format == "\n") debug_putc('\n');
            else if(*format == "\r") debug_putc('\r');
            else if(*format == "\t") debug_putc('\t');
            else debug_putc(*format);
        }
        format++;
    }

    // debug_print("\n");
    va_end(argp);
}