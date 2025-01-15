/**
 * @file logger.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-10-22
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <graphics.h>
#include <opengl/glbackend.h>

static const char hex_digits[] = "0123456789abcdef";
static const char caps_hex_digits[] = "0123456789ABCDEF";

/**
 * @brief Display a warning message.
 *
 * This function displays a warning message on the console with color formatting.
 *
 * @param message The warning message to be displayed.
 * @param file The file name where the warning occurred.
 */
void warn(cstring message, cstring file) {
    cstring warn_message = yellow_color "[***] → " reset_color;
    print(warn_message);
    print(message);
    print(" at " blue_color);
    print(file);
    print(reset_color "\n");

    debug_print(warn_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");
}

/**
 * @brief Display an error message.
 *
 * This function displays an error message on the console with color formatting.
 *
 * @param message The error message to be displayed.
 * @param file The file name where the error occurred.
 */
void error(cstring message, cstring file) {
    cstring err_message = red_color "[***] → " reset_color;
    print(err_message);
    print(message);
    print(" at " blue_color);
    print(file);
    print(reset_color "\n");

    debug_print(err_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");
}

/**
 * @brief Display an informational message.
 *
 * This function displays an informational message on the console with color formatting.
 *
 * @param message The informational message to be displayed.
 * @param file The file name where the information is coming from.
 */
void info(cstring message, cstring file) {
    cstring info_message = blue_color "[***] → " reset_color;
    print(info_message);
    print(message);
    print(" at " blue_color);
    print(file);
    print(reset_color "\n");

    debug_print(info_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");
}

/**
 * @brief Display a success message.
 *
 * This function displays a success message on the console with color formatting.
 *
 * @param message The success message to be displayed.
 * @param file The file name associated with the success.
 */
void done(cstring message, cstring file) {
    cstring done_message = green_color "[***] → " reset_color;
    print(done_message);
    print(message);
    print(" at " blue_color);
    print(file);
    print(reset_color "\n");

    debug_print(done_message);
    debug_print(message);
    debug_print(" at " blue_color);
    debug_print(file);
    debug_print(reset_color "\n");
}

/**
 * @brief Prints a char, using print(&c);
 * 
 * @param c char to print
 * @note Internally using Flanterm's putchar function
 */
void __putc(char c) {
    char str[1];

    str[0] = c;
    str[1] = '\0';

    print(str);
}

/**
 * @brief Prints decimal numbers
 * 
 * @param num the number to be printed
 */
void printdec(int num) {
    if (num < 0) {
        putc('-');
        num = -num;
    } else if (num == 0) {
        putc('0');
        return;
    }

    char buf[21]; // Sufficient for a 64-bit number
    int i = sizeof(buf) - 1;
    buf[i] = '\0';

    while (num > 0) {
        buf[--i] = (num % 10) + '0';
        num /= 10;
    }

    print(&buf[i]);
}

void printdec_unsigned(unsigned int num) {
    if (num == 0) {
        putc('0');
        return;
    }

    char buf[21]; // Sufficient for a 64-bit unsigned number
    int i = sizeof(buf) - 1;
    buf[i] = '\0';

    while (num > 0) {
        buf[--i] = (num % 10) + '0';
        num /= 10;
    }

    print(&buf[i]);
}


/**
 * @brief Prints a value in binary format
 * 
 * @param value A pointer to the value that will be printed
 */
void printbin(uint8_t value)
{
    static char binaryRepresentation[9];
    binaryRepresentation[8] = 0;

    for (int i = 0; i < 8; i++)
        binaryRepresentation[i] = (value & 0x80 >> i) ? '1' : '0';
    
    print(binaryRepresentation);
}

/**
 * @brief Prints Hexadecimal number
 * 
 * @param hex the hexadecimal number to be printed.
 */
 char* hex_to_string(signed int num, bool caps) {    
    int i;
    char buf[21];
    signed int n = num;

    if(n < 0){
        n = -n;
    }

    if (!n) {
        print("00");
        return;
    }

    buf[16] = 0;

    for (i = 15; n; i--) {
        if(caps) buf[i] = caps_hex_digits[n % 16];
        else buf[i] = hex_digits[n % 16];

        n /= 16;
    }

    i++;
    return &buf[i];
}

/**
 * @brief Prints Hexadecimal number
 * 
 * @param hex the hexadecimal number to be printed.
 */
void printhex(signed int num, bool caps) {    
    int i;
    char buf[21];
    signed int n = num;

    if(n < 0){
        n = -n;
    }

    if (!n) {
        print("00");
        return;
    }

    buf[16] = 0;

    for (i = 15; n; i--) {
        if(caps) buf[i] = caps_hex_digits[n % 16];
        else buf[i] = hex_digits[n % 16];

        n /= 16;
    }

    i++;
    print(&buf[i]);
}

/**
 * @brief More uniform print function.
 * Supports any number of arguments (va_list)
 * 
 * @param format 
 * @param ... 
 */
void printf(cstring format, ...) {
    va_list argp;
    va_start(argp, format);

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            switch (*format)
            {
            case 'b':
                printbin(va_arg(argp, size_t));
                break;

            case 'x':
                printhex(va_arg(argp, size_t), no);
                break;
            
            case 'X':
                printhex(va_arg(argp, size_t), yes);
                break;
            
            case 'u':
                printdec_unsigned(va_arg(argp, size_t));
                break;
            
            case 'd':
                printdec(va_arg(argp, size_t));
                break;
            
            case 's':
                print(va_arg(argp, char*));
                break;

            case 'c':
                putc(va_arg(argp, char));
                break;
            }
        } else {
            if(*format == "\n") print("\n");
            else if(*format == "\r") print("\r");
            else if(*format == "\t") print("\t");
            else putc(*format);
        }
        format++;
    }

    print("\n");
    va_end(argp);
}

void print_bitmap(int x, int y, int w, int h, bool* pixels, int32 color) {
    int i, j, l;
    for (l = j = 0; l < h; l++) {
        for (i = 0; i < w; i++, j++) {
            if(pixels[j] == true) glWritePixel((uvec2){x + i, y + l}, color);
        }
    }
}
