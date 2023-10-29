/**
 * @file acpi.h
 * @author Mintsuki (https://github.com/mintsuki)
 * @brief The ACPI Header
 * @version 0.1
 * @date 2023-10-29
 * 
 * @copyright Copyright (C) 2019-2023 mintsuki and contributors.
 * 
 */
#ifndef __ACPI_H_
#define __ACPI_H_ 1

#include <stdint.h>
#include <stddef.h>

struct sdt {
    char signature[4];
    uint32_t length;
    uint8_t rev;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} __attribute__((packed));

/**
 * @brief Initializes and iterates through all ACPI tables
 * 
 */
void acpi_init();

/**
 * @brief Searches for an STD Header in the ACPI Tables
 * 
 * @param signature Signature of the requested STD Header
 * @param index The index of the requested SDT Header
 * @returns Address of the ACPI Table
 */
void *acpi_find_sdt(const char *signature, size_t index);

#endif