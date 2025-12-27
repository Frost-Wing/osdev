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
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;

    // ...
    uint16_t reset_register_io_port; // 0x64 in ACPI spec, but can vary
    uint8_t reset_value;             // value to write to reset
    // ...
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

void *acpi_find_sdt(const char *signature, size_t index)
{
    size_t cnt = 0;

    for (size_t i = 0; i < rsdt->sdt.length - sizeof(struct sdt); i++) {
        struct sdt *ptr;
        if (use_xsdt)
            ptr = (struct sdt *)(uintptr_t)((int64 *)rsdt->ptrs_start)[i];
        else
            ptr = (struct sdt *)(uintptr_t)((int32 *)rsdt->ptrs_start)[i];

        if (!strncmp(ptr->signature, signature, 4) && cnt++ == index)
            return (void *)ptr;
    }

    return NULL;
}

void acpi_reboot() {
    clear_interrupts();

    struct fadt* fadt_ptr = (struct fadt*)acpi_find_sdt("FACP", 0);
    if (!fadt_ptr) {
        meltdown_screen("FADT not found! Falling back to hard reset.", __FILE__, __LINE__, 0xdeadbeef);
        sleep(5);
        hard_reset();
    }

    // ACPI reset uses the reset register GAS
    struct acpi_gas* reg = (struct acpi_gas*)((uintptr_t)fadt_ptr + 0x64); // offset for reset reg in FADT
    if (!reg->address) {
        meltdown_screen("ACPI Reset register not present! Hard reset in 5 sec.", __FILE__, __LINE__, 0xdeadbeef);
        sleep(5);
        hard_reset();
    }

    // Only IO space is widely supported
    if (reg->address_space == 1) { // IO port
        outb((uint16_t)reg->address, fadt_ptr->reset_value);
    } else {
        meltdown_screen("ACPI Reset register not IO space! Hard reset.", __FILE__, __LINE__, 0xdeadbeef);
        hard_reset();
    }

    // If that fails
    meltdown_screen("ACPI Reset failed! Falling back to hard reset.", __FILE__, __LINE__, 0xfaded);
    hard_reset();
}


void hard_reset(void) {
    // Wait until input buffer is clear (bit 1 of 0x64)
    for (int i = 0; i < 100000; i++) {
        if (!(inb(0x64) & 0x02)) break;
    }

    // Request CPU reset via keyboard controller
    outb(0x64, 0xFE);

    // If we reach here, reset failed
    meltdown_screen("Hard reset failed! (unsupported hardware?)", __FILE__, __LINE__, 0xBADBED);

    // Halt forever
    hcf2();
}