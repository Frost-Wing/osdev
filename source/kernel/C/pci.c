/**
 * @file pci.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The PCI Code for the kernel
 * @version 0.1
 * @date 2023-10-23
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <pci.h>
#include <memory.h>
#include <filesystems/vfs.h>
#include <filesystems/layers/proc.h>

int gpu_index = 0;
int total_devices = 0;

typedef struct {
    int index;
} pci_proc_priv_t;

pci_proc_priv_t pci_privs[MAX_PCI_DEVICES];
procfs_entry_t pci_entries[MAX_PCI_DEVICES];

cstring display_adapter_name = "Frost Generic Display Adapter";
cstring GPUName[2] = {"Frost Generic Display Driver for Graphics Processing Unit"}; // Max 2 GPUs allowed

uint64* graphics_base_Address = null;
cstring using_graphics_card = "unknown";

/**
 * @brief Function to read a 32-bit value from the PCI configuration space
 * 
 * @param bus 
 * @param slot 
 * @param func 
 * @param offset 
 * @return uint32 
 */
uint32 pci_config_read_dword(uint8 bus, uint8 slot, uint8 func, uint8 offset) {
    uint32 address = (uint32)((1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | ((uint32_t)offset & 0xFCU));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write_dword(uint8 bus, uint8 slot, uint8 func, uint8 offset, uint32_t value) {
    uint32 address = (uint32)((1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | ((uint32_t)offset & 0xFCU));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

/**
 * @brief Gets the AHCI bar address
 * 
 * @param bus The PCI bus number.
 * @param slot The PCI slot number.
 * @param func The PCI function number.
 * @param bar_num The Base Address Register Number.
 * @return uint32 
 */
uint32 get_ahci_bar_address(uint8 bus, int slot, int func, int bar_num) {
    int bar_offset = 0x10 + (bar_num * 4);

    return pci_config_read_dword(bus, (uint8)slot, (uint8)func, (uint8)bar_offset);
}

/**
 * @brief Read a 16-bit value from a PCI configuration register.
 *
 * This function reads a 16-bit value from a specified PCI configuration register
 * using the provided bus, slot, function, and offset parameters.
 *
 * @param bus The PCI bus number.
 * @param slot The PCI slot number.
 * @param func The PCI function number.
 * @param offset The offset within the PCI configuration register to read.
 * @return The 16-bit value read from the specified PCI configuration register.
 */
uint16 pci_read_word(uint16 bus, uint16 slot, uint16 func, uint16 offset)
{
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16 tmp = 0;
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | ((uint32_t)offset & 0xfcU) | 0x80000000U);
    outl(0xCF8, address);
    tmp = (uint16)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
    return (tmp);
}


/**
 * @brief Gets the Vendor ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16 Vendor ID
 */
uint16 getVendorID(uint16 bus, uint16 device, uint16 function)
{
    uint32 r0 = pci_read_word(bus,device,function,0);
    return (uint16)r0;
}

/**
 * @brief Gets the Device ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16  Device ID
 */
uint16 getDeviceID(uint16 bus, uint16 device, uint16 function)
{
    uint32 r0 = pci_read_word(bus,device,function,2);
    return (uint16)r0;
}

uint8 getRevision(uint16 bus, uint16 slot, uint16 func) {
    return (uint8)(pci_read_word(bus, slot, func, 0x08) & 0xFFU);
}

uint8 getProgIF(uint16 bus, uint16 slot, uint16 func) {
    return (uint8)((pci_config_read_dword((uint8)bus, (uint8)slot, (uint8)func, 0x08) >> 8U) & 0xFFU);
}

/**
 * @brief Gets the Class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16 Class ID
 */
uint16 getClassId(uint16 bus, uint16 device, uint16 function)
{
    uint32 r0 = pci_read_word(bus,device,function,0xA);
    return (uint16)((r0 & 0xFF00U) >> 8U);
}

/**
 * @brief Gets the Sub-class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16 Sub-class ID
 */
uint16 getSubClassId(uint16 bus, uint16 device, uint16 function)
{
    uint32 r0 = pci_read_word(bus,device,function,0xA);
    return (uint16)(r0 & 0x00FFU);
}


char vendorNames[MAX_PCI_DEVICES][64];
char deviceNames[MAX_PCI_DEVICES][64];
char classNames[MAX_PCI_DEVICES][64];

uint16 vendors[MAX_PCI_DEVICES];
uint16 devices[MAX_PCI_DEVICES];
uint16 classes[MAX_PCI_DEVICES];
uint16 subclasses[MAX_PCI_DEVICES];
uint8 revisions[MAX_PCI_DEVICES];

pci_location_t pciLocations[MAX_PCI_DEVICES];
// PROCFS PCI
int proc_pci_device_read(
    vfs_file_t* file,
    uint8_t* buf,
    uint32_t size,
    void* priv
)
{
    pci_proc_priv_t* p = priv;
    int i = p->index;

    char tmp[512];

    int len = snprintf(
        tmp,
        sizeof(tmp),
        "Bus: %02x\n"
        "Slot: %02x\n"
        "Function: %x\n"
        "Vendor: %04x\n"
        "Device: %04x\n"
        "Class: %s\n"
        "Name: %s\n"
        "Revision: %02x\n",
        pciLocations[i].bus,
        pciLocations[i].slot,
        pciLocations[i].func,
        vendors[i],
        devices[i],
        classNames[i],
        deviceNames[i],
        revisions[i]
    );

    if (file->pos >= (uint32_t)len)
        return 0;

    uint32_t rem = len - file->pos;
    if (rem > size)
        rem = size;

    memcpy(buf, tmp + file->pos, rem);
    file->pos += rem;

    return rem;
}

/**
 * @brief Scans (Probes) PCI Devices
 * 
 */
void probe_pci(void){
    info("Probe has been started!", __FILE__);
    int i = 0;
    for(uint32 bus = 0; bus < 256; bus++)
    {
        for(uint32 slot = 0; slot < 32; slot++)
        {
            for(uint32 function = 0; function < 8; function++)
            {
                    uint16 vendor = getVendorID((uint16)bus, (uint16)slot, (uint16)function);
                    if(vendor == 0xffff) continue;
                    uint16 device = getDeviceID((uint16)bus, (uint16)slot, (uint16)function);
                    uint16 classid = getClassId((uint16)bus, (uint16)slot, (uint16)function);
                    uint16 subclassid = getSubClassId((uint16)bus, (uint16)slot, (uint16)function);
                    uint16 revision = getRevision((uint16)bus, (uint16)slot, (uint16)function);
                    uint8 prog_if = getProgIF((uint16)bus, (uint16)slot, (uint16)function);

                    cstring vendorName  = parse_vendor(vendor);
                    cstring className   = parse_class(classid);
                    char deviceName[64];

                    strcpy(deviceName, "Unknown Device");

                    const pci_id_entry_t* entry = pci_lookup(vendor, device, (uint8_t)classid);

                    if (entry) {
                        strncpy(deviceName, entry->name, sizeof(deviceName) - 1);

                        if (entry->is_gpu) {
                            /* assigned after device name is copied to stable storage */
                        }

                        if (entry->probe)
                            entry->probe((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                    } else if (classid == 0x03) {
                        cstring gpu = auto_name_gpu(vendor, device);
                        strncpy(deviceName, gpu, sizeof(deviceName) - 1);
                        /* assigned after device name is copied to stable storage */
                    } else {
                        snprintf(deviceName, sizeof(deviceName), "Unknown Device (0x%04X)", device);
                    }

                    deviceName[sizeof(deviceName) - 1] = 0;

                    if (classid == 0x01 && subclassid == 0x06 && prog_if == 0x01) {
                        done("AHCI controller detected (generic)", __FILE__);
                        probe_ahci((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                    } else if (classid == 0x01 && subclassid == 0x08 && prog_if == 0x02) {
                        done("NVMe controller detected (generic)", __FILE__);
                        probe_nvme((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                    }
                    
                    print(green_color);
                    printf("%9s : Device : %9s -- Class : %9s", vendorName, deviceName, className);
                    print(reset_color);
                    
                    debug_printf(green_color);
                    debug_printf("%s : Device : %s -- Class : %s\n", vendorName, deviceName, className);
                    debug_printf(reset_color);
                    
                    strncpy(vendorNames[i], vendorName, 63);
                    strncpy(deviceNames[i], deviceName, 63);
                    strncpy(classNames[i],  className,  63);
                    pciLocations[i] = (pci_location_t){(uint16)bus, (uint16)slot, (uint16)function};

                    vendors[i] = vendor;
                    devices[i] = device;
                    classes[i] = classid;
                    subclasses[i] = subclassid;
                    revisions[i] = (uint8)revision;

                    vendorNames[i][63] = 0;
                    deviceNames[i][63] = 0;
                    classNames[i][63]  = 0;
                    if (classid == 0x03 && gpu_index < 2) {
                        display_adapter_name = deviceNames[i];
                        GPUName[gpu_index++] = deviceNames[i];
                    }
                    i++;
            }
        }
    }

    done("Successfully completed probe!", __FILE__);
    done("Successfully saved to a array and verified.", __FILE__);

    total_devices = i;
    printf("Total PCI Devices : %02d", total_devices);
    printf(yellow_color "GPU(0) : %s", GPUName[0]);
    printf("GPU(1) : %s" reset_color,  GPUName[1]);
    printf("Display Adapter : %s", display_adapter_name);
}

int proc_pci_register() {
    for (int i = 0; i < total_devices; i++) {
        static char names[MAX_PCI_DEVICES][32];

        snprintf(
            names[i],
            sizeof(names[i]),
            "pci/%02x:%02x.%x",
            pciLocations[i].bus,
            pciLocations[i].slot,
            pciLocations[i].func
        );

        pci_privs[i].index = i;

        pci_entries[i].name = names[i];
        pci_entries[i].type = PROC_FILE;
        pci_entries[i].read = proc_pci_device_read;
        pci_entries[i].write = NULL;
        pci_entries[i].priv = &pci_privs[i];

        procfs_register(&pci_entries[i]);
    }

    return 0;
}

int proc_pci_devices_read(
    vfs_file_t* file,
    uint8_t* buf,
    uint32_t size,
    void* priv
)
{
    (void)priv;

    static char tmp[4096];
    int len = 0;

    for (int i = 0; i < total_devices; i++) {
        len += snprintf(
            tmp + len,
            sizeof(tmp) - len,
            "%02x:%02x.%x %04x %04x %s\n",
            pciLocations[i].bus,
            pciLocations[i].slot,
            pciLocations[i].func,
            vendors[i] & 0xffff,
            devices[i] & 0xffff,
            deviceNames[i]
        );
    }

    if (file->pos >= (uint32_t)len)
        return 0;

    uint32_t rem = len - file->pos;
    if (rem > size)
        rem = size;

    memcpy(buf, tmp + file->pos, rem);
    file->pos += rem;

    return rem;
}


void print_lspci(void) {
    for (int i = 0; i < total_devices; i++) {
            printf("%02d:%2x.%d " yellow_color "%s " green_color "%s " red_color "%s" reset_color " (rev %02x)",
                pciLocations[i].bus,
                pciLocations[i].slot, 
                pciLocations[i].func,
                vendorNames[i],
                deviceNames[i],
                classNames[i],
                revisions[i]);
    }
}
