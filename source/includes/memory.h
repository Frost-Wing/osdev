/**
 * @file memory.h
 * @brief Custom memory manipulation functions.
 *
 * This header file defines custom memory manipulation functions that are
 * equivalent to the standard C library functions `memcpy`, `memset`,
 * `memmove`, and `memcmp`.
 */

 
#ifndef MEMORY_H
#define MEMORY_H

#include <basics.h>
#include <limine.h>

struct memory_context {
    int64 total;
    int64 usable;
    int64 reserved;
    int64 acpi_reclaimable;
    int64 acpi_nvs;
    int64 bad;
    int64 bootloader_reclaimable;
    int64 kernel_modules;
    int64 framebuffer;            // Mostly unneeded because frame buffer struct separately gives it,
    int64 unknown;                // This value must be always 0.
};

/**
 * @brief Copies a block of memory from a source location to a destination location.
 *
 * This function copies 'n' bytes of memory from 'src' to 'dest'. The source and destination
 * memory areas should not overlap.
 *
 * @param dest Pointer to the destination memory location.
 * @param src Pointer to the source memory location.
 * @param n Number of bytes to copy.
 * @return A pointer to the destination memory location 'dest'.
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * @brief Sets a block of memory to a specified value.
 *
 * This function sets the first 'n' bytes of the memory block at 's' to the value 'c'.
 *
 * @param s Pointer to the memory location.
 * @param c Value to set (as an integer).
 * @param n Number of bytes to set.
 * @return A pointer to the memory location 's'.
 */
void *memset(void *s, int c, size_t n);

/**
 * @brief Copies a block of memory from a source location to a destination location, possibly overlapping.
 *
 * This function copies 'n' bytes of memory from 'src' to 'dest'. The source and destination
 * memory areas may overlap. In such cases, it ensures the correct copy by choosing the appropriate
 * copy direction.
 *
 * @param dest Pointer to the destination memory location.
 * @param src Pointer to the source memory location.
 * @param n Number of bytes to copy.
 * @return A pointer to the destination memory location 'dest'.
 */
void *memmove(void *dest, const void *src, size_t n);

/**
 * @brief Compares two blocks of memory.
 *
 * This function compares the first 'n' bytes of memory in 's1' and 's2'.
 *
 * @param s1 Pointer to the first memory location.
 * @param s2 Pointer to the second memory location.
 * @param n Number of bytes to compare.
 * @return 0 if the memory blocks are equal, a negative value if 's1' is less than 's2,
 * and a positive value if 's1' is greater than 's2'.
 */
int memcmp(const void *s1, const void *s2, size_t n);

/**
 * @brief Dump memory content from start to end.
 *
 * This function iterates through the memory addresses from start to end
 * and prints the address and corresponding value in hexadecimal format.
 *
 * @param start Pointer to the start address of the memory to be dumped.
 * @param end   Pointer to the end address of the memory to be dumped.
 */
void memory_dump(const void* start, const void* end);

/**
 * @brief Allocates memory at an specific address.
 * 
 * @param phys_addr Physical address to allocate memory.
 * @param size Size of the memory to allocate.
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
void* allocate_memory_at_address(int64 phys_addr, size_t size);

/**
 * @brief Display the formatted memory context.
 * 
 * @param memory The memory context to display.
 */
void display_memory_formatted(struct memory_context* memory);

/**
 * @brief Reads the Limine memory map and saves to an usable context.
 * 
 * @param memory The memory context.
 * @param memory_map_request Limine Memory Mam Request.
 */
void analyze_memory_map(struct memory_context* memory, volatile struct limine_memmap_request memory_map_request);

/**
 * @brief Returns the CR2 register.
 * 
 * @return uint64_t value of CR2.
 */
uint64_t getCR2();

#endif // MEMORY_H