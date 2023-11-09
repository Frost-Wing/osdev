/**
 * @file sse.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief SSE Support for Wing kernel
 * @version 0.1
 * @date 2023-10-27
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */

#include <sse.h>
#define SSE_XMM_SIZE 512

char fxsave_region[512] __attribute__((aligned(16)));

/**
 * @brief Loads the SEE fully with fxsave
 * 
 */
void load_complete_sse(){
    asm volatile(" fxsave %0 "::"m"(fxsave_region));
    done("Completely loaded SSE!", __FILE__);
}

/**
 * @brief Checks if CPU is compatible with SSE
 * 
 */
void check_sse(){
    int eax = 0x1;
    int ebx, ecx, edx;
    asm volatile("cpuid"
                     : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                     : "a" (eax));

    bool noSSE = !(edx & (1 << 25));

    if (noSSE) {
        warn("SSE Not detected!", __FILE__);
    }else{
        int sseVersion = (ecx >> 25) & 0x7;
        // print("SSE Version: ");
        // printf("%d", sseVersion);
        done("SSE detected!", __FILE__);
    }
}