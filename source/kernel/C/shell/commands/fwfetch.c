/**
 * @file fwfetch.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief A neofetch clone for fwfetch
 * @version 0.1
 * @date 2025-01-15
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <basics.h>
#include <cpuid2.h>
#include <graphics.h>

void fwfetch(){
    print(blue_color);
    printf("OS       : %s", OS_NAME);
    printf("CPU      : %s", get_cpu_vendor());
    printf("Terminal : %s", "fsh");

    #if defined (__x86_64__)
    print("Arch     : x86_64");
    #elif defined (__aarch64__) || defined (__riscv)
    print("Arch     : RISC-V");
    #elif defined (__arm__) || defined (__aarch32__)
    print("Arch     : ARM");
    #endif
    print(reset_color);
}