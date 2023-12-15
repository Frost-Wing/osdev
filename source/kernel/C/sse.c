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
    #if defined (__x86_64__)
        asm volatile (
            "mov %%cr0, %%rax\n"
            "and $0xFFFB, %%ax\n"
            "or $0x2, %%ax\n"
            "mov %%rax, %%cr0\n"
            "mov %%cr4, %%rax\n"
            "or $0x600, %%ax\n"  // Set bit 9 (OSFXSR) and bit 10 (OSXMMEXCPT)
            "mov %%rax, %%cr4\n"
            :
            :
            : "rax"
        );
        asm volatile(" fxsave %0 "::"m"(fxsave_region));
        done("Completely loaded SSE!", __FILE__);
    #endif
}

/**
 * @brief Checks if CPU is compatible with SSE
 * 
 */
void check_sse(){
    #if defined (__x86_64__)
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
            done("SSE detected!", __FILE__);
            printf("SSE Version : %d", sseVersion);
        }
    #endif
}