/**
 * @file cpuid2.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The CPUID instructions
 * @version 0.1
 * @date 2023-11-10
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>
#include <graphics.h>

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
static inline int cpuid_string(int code, int where[4]);

/**
 * @brief Returns a string representing the CPU type.
 *
 * This function queries the CPU type using the cpuid instruction and returns
 * a string representation of the CPU type.
 *
 * @return A constant string representing the CPU type.
 */
cstring cpu_string();

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
inline void cpuid(int32 reg, int32 *eax, int32 *ebx, int32 *ecx, int32 *edx);

/**
 * @brief Retrieve the CPU vendor string.
 * 
 * @return The CPU vendor string.
 */
cstring get_cpu_vendor();

/**
 * @brief Retrieve the CPU name.
 * 
 * @return The CPU name.
 */
cstring get_cpu_name();

/**
 * @brief Print CPU information, including the vendor and CPU string.
 */
void print_cpu_info();

/**
 * @brief Retrieve and print L1 cache information.
 */
void print_L1_cache_info();

/**
 * @brief Retrieve and print L2 cache information.
 */
void print_L2_cache_info();

/**
 * @brief Retrieve and print L3 cache information.
 */
void print_L3_cache_info();