#include <cpuid2.h>

/**
 * @brief Executes the CPUID instruction with the given code and stores the results.
 *
 * This function executes the CPUID instruction with the specified code and stores
 * the results in the provided array `where`.
 *
 * @param code The CPUID code to query.
 * @param where An integer array to store the result, with at least 4 elements.
 * @return The value of the EAX register after executing CPUID.
 */
static inline int cpuid_string(int code, int where[4]) {
    __asm__ volatile ("cpuid":"=a"(*where),"=b"(*(where+0)),
               "=d"(*(where+1)),"=c"(*(where+2)):"a"(code));
    return (int)where[0];
}

/**
 * @brief Returns a string representing the CPU type.
 *
 * This function queries the CPU type using the cpuid instruction and returns
 * a string representation of the CPU type.
 *
 * @return A constant string representing the CPU type.
 */
cstring const cpu_string() {
    static char s[16] = "BogusProces!";
    cpuid_string(0, (int*)(s));
    return s;
}

/**
 * @brief Executes the CPUID instruction and retrieves values of the specified registers.
 *
 * This function executes the CPUID instruction with the given register and stores
 * the values in the provided pointers for EAX, EBX, ECX, and EDX registers.
 *
 * @param reg The register to query with CPUID.
 * @param eax Pointer to store the value of EAX register.
 * @param ebx Pointer to store the value of EBX register.
 * @param ecx Pointer to store the value of ECX register.
 * @param edx Pointer to store the value of EDX register.
 */
inline void cpuid(int32 reg, int32 *eax, int32 *ebx, int32 *ecx, int32 *edx) {
    __asm__ volatile("cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "0" (reg));
}

/**
 * @brief Retrieve the CPU vendor string.
 * 
 * @return The CPU vendor string.
 */
cstring get_cpu_vendor() {
    static char vendor[128];
    cpuid(0x80000002, (int32 *)(vendor +  0), (int32 *)(vendor +  4), (int32 *)(vendor +  8), (int32 *)(vendor + 12));
    cpuid(0x80000003, (int32 *)(vendor + 16), (int32 *)(vendor + 20), (int32 *)(vendor + 24), (int32 *)(vendor + 28));
    cpuid(0x80000004, (int32 *)(vendor + 32), (int32 *)(vendor + 36), (int32 *)(vendor + 40), (int32 *)(vendor + 44));
    vendor[127] = 0;
    return vendor;
}

/**
 * @brief Print CPU information, including the vendor and CPU string.
 */
void print_cpu_info() {
    printf("CPU Vendor: %s%nCPU String: %s", get_cpu_vendor(), cpu_string());
}

/**
 * @brief Retrieve and print L1 cache information.
 */
void print_L1_cache_info() {
    int32 eax, ebx, ecx, edx;
    cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    if ((edx & 0xFF) == 0) {
        print("L1 Cache not present.\n");
        return;
    }
    printf("CPU Line Size: %d B, Assoc. Type: %d; Cache Size: %d KB. (L1 INFO)", ecx & 0xff, (ecx >> 12) & 0x07, (ecx >> 16) & 0xffff);
}

/**
 * @brief Retrieve and print L2 cache information.
 */
void print_L2_cache_info() {
    int32 eax, ebx, ecx, edx;
    cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    if ((edx & 0xFF) == 0) {
        print("L2 Cache not present.\n");
        return;
    }
    printf("CPU Line Size: %d B, Assoc. Type: %d; Cache Size: %d KB. (L2 INFO)", ecx & 0xff, (ecx >> 12) & 0x0F, (ecx >> 16) & 0xFFFF);
}

/**
 * @brief Retrieve and print L3 cache information.
 */
void print_L3_cache_info() {
    int32 eax, ebx, ecx, edx;
    cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
    if ((edx & 0xFF) == 0) {
        print("L3 Cache not present.\n");
        return;
    }
    printf("CPU Line Size: %d B, Assoc. Type: %d; Cache Size: %d KB. (L3 INFO)", edx & 0xff, (edx >> 12) & 0x0F, (edx >> 16) & 0xFFFF);
}
