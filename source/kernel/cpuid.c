#include <stdint.h>
#include <stddef.h>
 
static inline int cpuid_string(int code, int where[4]) {
  __asm__ volatile ("cpuid":"=a"(*where),"=b"(*(where+0)),
               "=d"(*(where+1)),"=c"(*(where+2)):"a"(code));
  return (int)where[0];
}
 
const char * const cpu_string() {
	static char s[16] = "BogusProces!";
	cpuid_string(0, (int*)(s));
	return s;
}
inline void cpuid(uint32_t reg, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "0" (reg));
}

char vendor[128];
char* get_cpu_name() {
    cpuid(0x80000002, (uint32_t *)(vendor +  0), (uint32_t *)(vendor +  4), (uint32_t *)(vendor +  8), (uint32_t *)(vendor + 12));
    cpuid(0x80000003, (uint32_t *)(vendor + 16), (uint32_t *)(vendor + 20), (uint32_t *)(vendor + 24), (uint32_t *)(vendor + 28));
    cpuid(0x80000004, (uint32_t *)(vendor + 32), (uint32_t *)(vendor + 36), (uint32_t *)(vendor + 40), (uint32_t *)(vendor + 44));
    vendor[127] = 0;
    return vendor;
}

void print_cpu(){
    printf("\033[1;34mCPU Vendor: \033[1;32m%s\n\033[1;34mCPU String: \033[1;32m%s%s", vendor, cpu_string(),"\033[0m");
}

void L1_cache_size() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    if ((edx & 0xFF) == 0) {
        print("L1 Cache not present.\n");
        return;
    }
    printf("\033[1;34mCPU Line Size: \033[1;32m%d B, \033[1;34mAssoc. Type: \033[1;32m%d; \033[1;34mCache Size: \033[1;32m%d KB. \033[0m(L1 INFO)", ecx & 0xff, (ecx >> 12) & 0x07, (ecx >> 16) & 0xffff);
}

void L2_cache_size() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    if ((edx & 0xFF) == 0) {
        print("L2 Cache not present.\n");
        return;
    }
    printf("\033[1;34mCPU Line Size: \033[1;32m%d B, \033[1;34mAssoc. Type: \033[1;32m%d; \033[1;34mCache Size: \033[1;32m%d KB. \033[0m(L2 INFO)", ecx & 0xff, (ecx >> 12) & 0x0F, (ecx >> 16) & 0xFFFF);
}

void L3_cache_size() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    if ((edx & 0xFF) == 0) {
        print("L3 Cache not present.\n");
        return;
    }
    printf("\033[1;34mCPU Line Size: \033[1;32m%d B, \033[1;34mAssoc. Type: \033[1;32m%d; \033[1;34mCache Size: \033[1;32m%d KB. \033[0m(L3 INFO)", edx & 0xff, (edx >> 12) & 0x0F, (edx >> 16) & 0xFFFF);
}