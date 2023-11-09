/**
 * @file acpi.c
 * @author Mintsuki (https://github.com/mintsuki)
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

struct rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t rev;
    uint32_t rsdt_addr;
    // ver 2.0 only
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct rsdt {
    struct sdt sdt;
    symbol ptrs_start;
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
        warn(" XSDT Not found! But we can ignore that!", __FILE__);
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
            ptr = (struct sdt *)(uintptr_t)((uint64_t *)rsdt->ptrs_start)[i];
        else
            ptr = (struct sdt *)(uintptr_t)((uint32_t *)rsdt->ptrs_start)[i];

        if (!strncmp(ptr->signature, signature, 4) && cnt++ == index)
            return (void *)ptr;
    }

    return NULL;
}