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
        print("SSE Version: ");
        printf("%d", sseVersion);
        done("SSE detected!", __FILE__);
    }
}

/**
 * @brief Original source from https://wiki.osdev.org/User:01000101/optlib/
 * 
 * @param m_start 
 * @param m_count 
 * @return const void* 
 */
static const void * memclr_sse2(const void * const m_start, const size_t m_count)
{
    // "i" is our counter of how many bytes we've cleared
    size_t i;

    // find out if "m_start" is aligned on a SSE_XMM_SIZE boundary
    if((size_t)m_start & (SSE_XMM_SIZE - 1))
    {
        i = 0;

        // we need to clear byte-by-byte until "m_start" is aligned on an SSE_XMM_SIZE boundary
        // ... and lets make sure we don't copy 'too' many bytes (i < m_count)
        while(((size_t)m_start + i) & (SSE_XMM_SIZE - 1) && i < m_count)
        {
            asm("stosb;" :: "D"((size_t)m_start + i), "a"(0));
            i++;
        }
    }
    else
    {
        // if "m_start" was aligned, set our count to 0
        i = 0;
    }
 
    // clear 64-byte chunks of memory (4 16-byte operations)
    for(; i + 64 <= m_count; i += 64)
    {
        asm volatile(" pxor %%xmm0, %%xmm0;	"    // set XMM0 to 0
                     " movdqa %%xmm0, 0(%0);	"    // move 16 bytes from XMM0 to %0 + 0
                     " movdqa %%xmm0, 16(%0);	"
                     " movdqa %%xmm0, 32(%0);	"
                     " movdqa %%xmm0, 48(%0);	"
                     :: "r"((size_t)m_start + i));
    }
 
    // copy the remaining bytes (if any)
    asm(" rep stosb; " :: "a"((size_t)(0)), "D"(((size_t)m_start) + i), "c"(m_count - i));

    // "i" will contain the total amount of bytes that were actually transfered
    i += m_count - i;

    // we return "m_start" + the amount of bytes that were transfered
    return (void *)(((size_t)m_start) + i);
}
