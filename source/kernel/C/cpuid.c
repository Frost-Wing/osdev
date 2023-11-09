#include <stdint.h>
#include <stddef.h>
#include <basics.h>

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
inline void cpuid(int32 reg, int32 *eax, int32 *ebx, int32 *ecx, int32 *edx)
{
    __asm__ volatile("cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "0" (reg));
}

char vendor[128];
char* get_cpu_name() {
    cpuid(0x80000002, (int32 *)(vendor +  0), (int32 *)(vendor +  4), (int32 *)(vendor +  8), (int32 *)(vendor + 12));
    cpuid(0x80000003, (int32 *)(vendor + 16), (int32 *)(vendor + 20), (int32 *)(vendor + 24), (int32 *)(vendor + 28));
    cpuid(0x80000004, (int32 *)(vendor + 32), (int32 *)(vendor + 36), (int32 *)(vendor + 40), (int32 *)(vendor + 44));
    vendor[127] = 0;
    return vendor;
}

void print_cpu(){
    printf("CPU Vendor: %s\nCPU String: %s", vendor, cpu_string());
}

void L1_cache_size() {
    int32 eax, ebx, ecx, edx;
    cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    if ((edx & 0xFF) == 0) {
        print("L1 Cache not present.\n");
        return;
    }
    printf("CPU Line Size: %d B, Assoc. Type: %d; Cache Size: %d KB. (L1 INFO)", ecx & 0xff, (ecx >> 12) & 0x07, (ecx >> 16) & 0xffff);
}

void L2_cache_size() {
    int32 eax, ebx, ecx, edx;
    cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    if ((edx & 0xFF) == 0) {
        print("L2 Cache not present.\n");
        return;
    }
    printf("CPU Line Size: %d B, Assoc. Type: %d; Cache Size: %d KB. (L2 INFO)", ecx & 0xff, (ecx >> 12) & 0x0F, (ecx >> 16) & 0xFFFF);
}

void L3_cache_size() {
    int32 eax, ebx, ecx, edx;
    cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    if ((edx & 0xFF) == 0) {
        print("L3 Cache not present.\n");
        return;
    }
    printf("CPU Line Size: %d B, Assoc. Type: %d; Cache Size: %d KB. (L3 INFO)", edx & 0xff, (edx >> 12) & 0x0F, (edx >> 16) & 0xFFFF);
}