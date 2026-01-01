/**
 * @file acpi.c
 * @author Pradosh & Mintsuki (https://github.com/mintsuki)
 * @brief The full ACPI Source
 * @version 0.1
 * @date 2023-10-29
 * 
 * @copyright Copyright (C) 2019-2023 mintsuki and contributors.
 * 
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <strings.h>
#include <kernel.h>
#include <memory.h>
#include <acpi.h>
#include <limine.h>
// #include <meltdown.h>

typedef char symbol[];

struct rsdp {
    char signature[8];
    int8 checksum;
    char oem_id[6];
    int8 rev;
    int32 rsdt_addr;
    // ver 2.0 only
    int32 length;
    int64 xsdt_addr;
    int8 ext_checksum;
    int8 reserved[3];
} __attribute__((packed));

struct rsdt {
    struct sdt sdt;
    symbol ptrs_start;
} __attribute__((packed));

struct fadt {
    struct sdt header;

    uint32_t firmware_ctrl;
    uint32_t dsdt;

    uint8_t  reserved1;
    uint8_t  preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_cnt;

    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;

    /* skip tons of fields */

    uint8_t  reserved2[76];

    struct acpi_gas reset_reg;   // ✅ correct
    uint8_t         reset_value;
} __attribute__((packed));


static bool use_xsdt;
static struct rsdt *rsdt;

char* oem_name = "";

bool virtualized = false;

static volatile struct limine_rsdp_request rsdp_req = {
    LIMINE_RSDP_REQUEST, 0, NULL
};

void acpi_init()
{
    struct rsdp *rsdp = rsdp_req.response->address;
    info("Found RSDP address!", __FILE__);

    if (rsdp->rev >= 2 && rsdp->xsdt_addr)
    {
        use_xsdt = true;
        rsdt = (struct rsdt *)((uintptr_t)rsdp->xsdt_addr);
        info("Using XSDT!", __FILE__);
    }
    else
    {
        use_xsdt = false;
        rsdt = (struct rsdt *)((uintptr_t)rsdp->rsdt_addr);
        warn("XSDT Not found! But we can ignore that!", __FILE__);
    }

    done("Successfully loaded!", __FILE__);
    oem_name = rsdp->oem_id;
    printf("OEM Name: %s", oem_name);
    if(contains(oem_name, "BOCHS") || contains(oem_name, "VBOX") || contains(oem_name, "QEMU") || contains(oem_name, "VMWARE")){
        warn("Currently running in a virtualized environment.", __FILE__);
        virtualized = true;
    }else{
        info("Currently running in a real computer.", __FILE__);
        virtualized = false;
    }
}

void *acpi_find_sdt(const char *signature, size_t index) {
    size_t entries = (rsdt->sdt.length - sizeof(struct sdt)) /
                     (use_xsdt ? 8 : 4);

    size_t found = 0;

    for (size_t i = 0; i < entries; i++) {
        struct sdt *ptr =
            use_xsdt ?
            (struct sdt *)(uintptr_t)((uint64_t *)rsdt->ptrs_start)[i] :
            (struct sdt *)(uintptr_t)((uint32_t *)rsdt->ptrs_start)[i];

        if (!strncmp(ptr->signature, signature, 4)) {
            if (found++ == index)
                return ptr;
        }
    }
    return NULL;
}

void acpi_reboot(uintptr_t hhdm_offset) {
    clear_interrupts();

    struct fadt *fadt = (struct fadt *)((uintptr_t)acpi_find_sdt("FACP", 0) + hhdm_offset);
    if (!fadt) goto fallback;

    struct acpi_gas *reg = &fadt->reset_reg;

    if (reg->address && reg->address_space == 1) {
        printf("ACPI reset: space=%d addr=0x%x value=0x%x", reg->address_space, reg->address, fadt->reset_value);
        if (reg->address > 0xFFFF) 
            printf("Invalid I/O port! Will not work.");

        outb((uint16_t)reg->address, fadt->reset_value);
    }

fallback:
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    
    // Keyboard controller reset
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 0x02)) break;
    outb(0x64, 0xFE);

    // Triple fault fallback (works everywhere)
    asm volatile (
        "lidt (0)\n"
        "int $3\n"
    );

    hcf2();
}
