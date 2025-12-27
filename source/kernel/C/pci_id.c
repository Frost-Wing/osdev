/**
 * @file pci_id.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The list of all officially recognized devices.
 * @version 0.1
 * @date 2025-12-27
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <pci.h>
#include <pci_id.h>

static pci_id_entry_t pci_ids[] = {

    // --- Intel chipset ---
    {0x8086, 0x29C0, 0xFF, "(Intel) Express DRAM Controller", 0, NULL},
    {0x8086, 0x2918, 0xFF, "(Intel) LPC Interface Controller", 0, NULL},
    {0x8086, 0x2922, 0xFF, "(Intel) 6 port SATA Controller [AHCI mode]", 0, probe_ahci},
    {0x8086, 0x2930, 0xFF, "(Intel) SMBus Controller", 0, NULL},

    // --- AMD chipset ---
    {0x1022, 0x7800, 0xFF, "AMD SATA Controller [AHCI]", 0, probe_ahci},
    {0x1022, 0x7801, 0xFF, "AMD SATA Controller [IDE]",  0, NULL},
    {0x1022, 0x7802, 0xFF, "AMD SATA Controller [RAID]", 0, NULL},
    {0x1022, 0x780B, 0xFF, "AMD SMBus Controller", 0, NULL},
    {0x1022, 0x780E, 0xFF, "AMD PCI Bridge", 0, NULL},
    {0x1022, 0x7812, 0xFF, "AMD USB EHCI Controller", 0, NULL},
    {0x1022, 0x7814, 0xFF, "AMD USB OHCI Controller", 0, NULL},

    // --- AMD (new) chipset ---
    {0x1022, 0x7901, 0xFF, "AMD Promontory SATA Controller", 0, probe_ahci},
    {0x1022, 0x790B, 0xFF, "AMD Promontory SMBus Controller", 0, NULL},
    {0x1022, 0x790E, 0xFF, "AMD PCI Express Bridge", 0, NULL},

    // --- Network ---
    {0x10EC, 0x8139, 0xFF, "RTL8139 Networking Card", 0, probe_rtl8139},
    {0x8086, 0x100e, 0xFF, "82540EM Gigabit Ethernet Controller", 0, NULL},

    // --- AMD GPUs ---
    {0x1002, 0x1028, 0x03, "AMD Radeon R7 M440/M445 (Radeon 530)", 1, NULL},
    {0x1002, 0x0128, 0x03, "AMD Radeon R7 465X", 1, NULL},
    {0x1002, 0x6900, 0x03, "Radeon R7 Mobile Series", 1, NULL},

    // --- Intel GPUs ---
    {0x8086, 0x1606, 0x03, "Intel(R) HD Graphics", 1, NULL},
    {0x8086, 0x1616, 0x03, "Intel(R) HD Graphics 5500", 1, NULL},
    {0x8086, 0x1912, 0x03, "Intel(R) HD Graphics 530", 1, NULL},
    {0x8086, 0x3e92, 0x03, "Intel(R) UHD Graphics 630", 1, NULL},
    {0x8086, 0x5917, 0x03, "Intel(R) UHD Graphics 620", 1, NULL},

    // --- NVIDIA GPUs ---
    {0x10DE, 0x0040, 0x03, "NV40 [GeForce 6800 Ultra]", 1, NULL},
    {0x10DE, 0x0090, 0x03, "G70 [GeForce 7800 GTX]", 1, NULL},
    {0x10DE, 0x1b83, 0x03, "GP104 [GeForce GTX 1060 6GB]", 1, NULL},
    {0x10DE, 0x0040, 0x03, "NV40 [GeForce 6800 Ultra]", 1, NULL},
    {0x10DE, 0x0041, 0x03, "NV40 [GeForce 6800]",       1, NULL},
    {0x10DE, 0x0042, 0x03, "NV40 [GeForce 6800 LE]",    1, NULL},
    {0x10DE, 0x0043, 0x03, "NV40 [GeForce 6800 XE]",    1, NULL},
    {0x10DE, 0x0044, 0x03, "NV40 [GeForce 6800 XT]",    1, NULL},
    {0x10DE, 0x0045, 0x03, "NV40 [GeForce 6800 GT]",    1, NULL},
    {0x10DE, 0x0046, 0x03, "NV40 [GeForce 6800 GS]",    1, NULL},

    {0x10DE, 0x0090, 0x03, "G70 [GeForce 7800 GTX]",    1, NULL},
    {0x10DE, 0x0091, 0x03, "G70 [GeForce 7800 GTX]",    1, NULL},
    {0x10DE, 0x0092, 0x03, "G70 [GeForce 7800 GT]",     1, NULL},
    {0x10DE, 0x0093, 0x03, "G70 [GeForce 7800 GS]",     1, NULL},
    {0x10DE, 0x0094, 0x03, "G70 [GeForce 7800 SLI]",    1, NULL},

    {0x10DE, 0x1B83, 0x03, "GP104 [GeForce GTX 1060 6GB]", 1, NULL},
    {0x10DE, 0x1B84, 0x03, "GP104 [GeForce GTX 1060 3GB]", 1, NULL},

    // --- Virtual GPUs ---
    {0x1b36, 0x0100, 0x03, "QXL Paravirtual Graphics Card", 1, NULL},
    {0x1af4, 0x1050, 0x03, "Virtio Graphics Card", 1, NULL},
    {0x5549, 0x0405, 0x03, "VMware SVGA Graphics", 1, NULL},
    {0x1013, 0x00b8, 0x03, "Cirrus Graphics", 1, NULL},

    {0, 0, 0, NULL, 0, NULL}
};

string parse_vendor(int16 vendor){
    string vendorName;

    switch (vendor) {
        case 0x8086:
        case 0x8087:
            vendorName = "Intel Corp.";
            break;
        case 0x03e7:
            vendorName = "Intel";
            break;
        case 0x10DE:
            vendorName = "NVIDIA";
            break;
        case 0x1002:
            vendorName = "AMD";
            break;
        case 0x1234:
            vendorName = "Brain Actuated Technologies";
            break;
        case 0x168c:
            vendorName = "Qualcomm Atheros";
            break;
        case 0x10EC:
            vendorName = "Realtek Semiconductor Co., Ltd.";
            break;
        case 0x15ad:
            vendorName = "VMware";
            break;
        case 0x1af4:
        case 0x1b36:
            vendorName = "Red Hat, Inc.";
            break;
        default:
            unsigned char str[20];
            itoa(vendor, str, sizeof(str), 16);
            vendorName = str;
    }

    return vendorName;
}

string parse_class(int16 classid){
    string className;

    switch (classid) {
        case 0x01:
            className = "Mass Storage Controller";
            break;
        case 0x02:
            className = "Network Controller";
            break;
        case 0x03:
            className = "Display Controller";
            break;
        case 0x04:
            className = "Multimedia Controller";
            break;
        case 0x05:
            className = "Memory Controller";
            break;
        case 0x06:
            className = "Bridge Device";
            break;
        case 0x07:
            className = "Simple Communication Controller";
            break;
        case 0x08:
            className = "Base System Peripheral";
            break;
        case 0x09:
            className = "Input Device";
            break;
        case 0x0a:
            className = "Docking Station";
            break;
        case 0x0b:
            className = "Processor";
            break;
        case 0x0c:
            className = "Serial Bus Controller";
            break;
        case 0x0d:
            className = "Wireless Controller";
            break;
        case 0x0e:
            className = "Intelligent Controller";
            break;
        case 0x0f:
            className = "Satellite Communication Controller";
            break;
        case 0x10:
            className = "Encryption/Decryption Controller";
            break;
        case 0x11:
            className = "Data Acquisition and Signal Processing Controller";
            break;
        case 0x12:
            className = "Processing Accelerator";
            break;
        case 0x13:
            className = "Non-Essential Instrumentation";
            break;
        case 0x40:
            className = "Video Device";
            break;
        case 0x80:
            className = "Unassigned";
            break;
        default:
            unsigned char str[20];
            itoa(classid, str, sizeof(str), 16);
            className = str;
    }

    return className;
}

const pci_id_entry_t* pci_lookup(uint16_t vendor, uint16_t device, uint8_t classid)
{
    for (int i = 0; pci_ids[i].name != null; i++) {
        if (pci_ids[i].vendor == vendor &&
            pci_ids[i].device == device &&
           (pci_ids[i].classid == 0xFF ||
            pci_ids[i].classid == classid)) {
            return &pci_ids[i];
        }
    }
    return NULL;
}

cstring auto_name_gpu(int16 vendor, int16 device)
{
    static char name[64];

    switch (vendor) {
        case 0x10DE:
            snprintf(name, sizeof(name), "NVIDIA GPU (0x%04X)", device);
            break;

        case 0x1002:
            snprintf(name, sizeof(name), "AMD GPU (0x%04X)", device);
            break;

        case 0x8086:
            snprintf(name, sizeof(name), "Intel GPU (0x%04X)", device);
            break;

        default:
            snprintf(name, sizeof(name),"Generic VGA Adapter (0x%04X)", device);
            break;
    }

    return name;
}

void probe_ahci(uint8_t bus, uint8_t slot, uint8_t function)
{
    ahci_hba_mem_t* abar = (ahci_hba_mem_t*)(pci_config_read_dword(bus, slot, function, 0x24) & ~0xF);

    if (abar && abar != (void*)0xFFFFFFFF) {
        done("Found AHCI BAR!", __FILE__);
        detect_ahci_devices(abar);
    } else {
        warn("Failed to find AHCI BAR!", __FILE__);
    }
}

void probe_rtl8139(uint8_t bus, uint8_t slot, uint8_t function)
{
    RTL8139->io_base = (uint16_t)(pci_read_word(bus, slot, function, RTL8139_IOADDR1) & 0xFFFC);

    int8_t irq = pci_read_word(bus, slot, function, RTL8139_IRQ_LINE);

    printf("Handler number : 0x%x", irq);
    registerInterruptHandler(irq, rtl8139_handler);
}
