/**
 * @file acpi-shutdown.c
 * @author Mintsuki (https://github.com/mintsuki)
 * @brief The ACPI Shutdown code for Wing kernel, this is not the full ACPI
 * @version 0.1
 * @date 2023-10-29
 * 
 * @copyright Copyright (C) 2019-2023 mintsuki and contributors.
 * 
 */
#include <stddef.h>
#include <stdint.h>
#include <acpi-shutdown.h>
#include <hal.h>

struct facp {
    char     signature[4];
    uint32 length;
    uint8  unneeded1[40 - 8];
    uint32 dsdt;
    uint8  unneeded2[48 - 44];
    uint32 SMI_CMD;
    uint8  ACPI_ENABLE;
    uint8  ACPI_DISABLE;
    uint8  unneeded3[64 - 54];
    uint32 PM1a_CNT_BLK;
    uint32 PM1b_CNT_BLK;
    uint8  unneeded4[89 - 72];
    uint8  PM1_CNT_LEN;
};

static inline uint8 parse_integer(uint8* s5_addr, uint64* value) {
    uint8 operation = *s5_addr++;

    switch(operation)
    {
        case 0x0: // ZeroOperation
            *value = 0;
            return 1;

        case 0x1: // 1 Op Byte
            *value = 1;
            return 1; // 1 Op Byte

        case 0xA: // ByteConst
            *value = s5_addr[0];
            return 2; // 1 Type Byte, 1 Data Byte

        case 0xB: // WordConst
            *value = s5_addr[0] | ((uint16)s5_addr[1] << 8);
            return 3; // 1 Type Byte, 3 Data Bytes

        case 0xC: // DWordConst
            *value = s5_addr[0] | ((uint32)s5_addr[1] << 8) | ((uint32)s5_addr[2] << 16) | ((uint32)s5_addr[3] << 24);
            return 5; // 1 Type Byte, 4 Data Bytes

        case 0xE: // QWordConst
            *value = s5_addr[0] | ((uint64)s5_addr[1] << 8) | ((uint64)s5_addr[2] << 16) | ((uint64)s5_addr[3] << 24) \
                | ((uint64)s5_addr[4] << 32) | ((uint64)s5_addr[5] << 40) | ((uint64)s5_addr[6] << 48) | ((uint64)s5_addr[7] << 56);
            return 9; // 1 Type Byte, 8 Data Bytes

        case 0xFF: // OnesOp
            *value = UINT64_MAX;
            return 1; // 1 Op Byte

        default:
            return 0; // No Integer, so something weird
    }
}

int acpi_shutdown_hack(uintptr_t direct_map_base, void *(*find_sdt)(cstring signature, size_t index)) {
    struct facp *facp = find_sdt("FACP", 0);

    uint8 *dsdt_ptr = (uint8 *)(uintptr_t)facp->dsdt + 36 + direct_map_base;
    size_t   dsdt_len = *((uint32 *)((uintptr_t)facp->dsdt + 4 + direct_map_base)) - 36;

    uint8 *s5_addr = 0;
    for (size_t i = 0; i < dsdt_len; i++) {
        if ((dsdt_ptr + i)[0] == '_'
         && (dsdt_ptr + i)[1] == 'S'
         && (dsdt_ptr + i)[2] == '5'
         && (dsdt_ptr + i)[3] == '_') {
            s5_addr = dsdt_ptr + i;
            goto s5_found;
        }
    }
    return -1;

s5_found:
    s5_addr += 4; // Skip last part of NameSeg, the earlier segments of the NameString were already tackled by the search loop
    if (*s5_addr++ != 0x12) // Assert that it is a PackageOp, if its a Method or something there's not much we can do with such a basic parser
        return -1;
    s5_addr += ((*s5_addr & 0xc0) >> 6) + 1; // Skip PkgLength
    if (*s5_addr++ < 2) // Make sure there are at least 2 elements, which we need, normally there are 4
        return -1;

    uint64 value = 0;
    uint8 size = parse_integer(s5_addr, &value);
    if (size == 0) // Wasn't able to parse it
        return -1;

    uint16 SLP_TYPa = (uint16)((value & 0x7ULL) << 10);
    s5_addr += size;


    size = parse_integer(s5_addr, &value);
    if (size == 0) // Wasn't able to parse it
        return -1;

    uint16 SLP_TYPb = (uint16)((value & 0x7ULL) << 10);
    s5_addr += size;

    if(facp->SMI_CMD != 0 && facp->ACPI_ENABLE != 0) { // This PC has SMM and we need to enable ACPI mode first
        outb((uint16)facp->SMI_CMD, facp->ACPI_ENABLE);
        for (int i = 0; i < 100; i++)
            inb(0x80);
    
        while ((inw((uint16)facp->PM1a_CNT_BLK) & 1U) == 0U)
            ;
    }
    

    outw((uint16)facp->PM1a_CNT_BLK, (uint16)(SLP_TYPa | (1U << 13)));
    if (facp->PM1b_CNT_BLK)
        outw((uint16)facp->PM1b_CNT_BLK, (uint16)(SLP_TYPb | (1U << 13)));

    for (int i = 0; i < 100; i++)
        inb(0x80);

    return -1;
}