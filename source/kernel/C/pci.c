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

int gpu_index = 0;
int total_devices = 0;

cstring display_adapter_name = "Frost Generic Display Adapter";
cstring GPUName[2] = {"Frost Generic Display Driver for Graphics Processing Unit"}; // Max 2 GPUs allowed

int64* graphics_base_Address = null;
cstring using_graphics_card = "unknown";

/**
 * @brief Function to read a 32-bit value from the PCI configuration space
 * 
 * @param bus 
 * @param slot 
 * @param func 
 * @param offset 
 * @return int32 
 */
int32 pci_config_read_dword(int8 bus, int8 slot, int8 func, int8 offset) {
    int32 address = (int32)((1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | ((uint32_t)offset & 0xFCU));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write_dword(int8 bus, int8 slot, int8 func, int8 offset, uint32_t value) {
    int32 address = (int32)((1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | ((uint32_t)offset & 0xFCU));
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
 * @return int32 
 */
int32 get_ahci_bar_address(int8 bus, int slot, int func, int bar_num) {
    int bar_offset = 0x10 + (bar_num * 4);

    return pci_config_read_dword(bus, (int8)slot, (int8)func, (int8)bar_offset);
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
int16 pci_read_word(int16 bus, int16 slot, int16 func, int16 offset)
{
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    int16 tmp = 0;
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | ((uint32_t)offset & 0xfcU) | 0x80000000U);
    outl(0xCF8, address);
    tmp = (int16)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
    return (tmp);
}


/**
 * @brief Gets the Vendor ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16 Vendor ID
 */
int16 getVendorID(int16 bus, int16 device, int16 function)
{
    int32 r0 = pci_read_word(bus,device,function,0);
    return (int16)r0;
}

/**
 * @brief Gets the Device ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16  Device ID
 */
int16 getDeviceID(int16 bus, int16 device, int16 function)
{
    int32 r0 = pci_read_word(bus,device,function,2);
    return (int16)r0;
}

int8 getRevision(int16 bus, int16 slot, int16 func) {
    return (int8)(pci_read_word(bus, slot, func, 0x08) & 0xFFU);
}

int8 getProgIF(int16 bus, int16 slot, int16 func) {
    return (int8)((pci_config_read_dword((int8)bus, (int8)slot, (int8)func, 0x08) >> 8U) & 0xFFU);
}

/**
 * @brief Gets the Class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16 Class ID
 */
int16 getClassId(int16 bus, int16 device, int16 function)
{
    int32 r0 = pci_read_word(bus,device,function,0xA);
    return (int16)((r0 & 0xFF00U) >> 8U);
}

/**
 * @brief Gets the Sub-class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16 Sub-class ID
 */
int16 getSubClassId(int16 bus, int16 device, int16 function)
{
    int32 r0 = pci_read_word(bus,device,function,0xA);
    return (int16)(r0 & 0x00FFU);
}

char vendorNames[MAX_PCI_DEVICES][64];
char deviceNames[MAX_PCI_DEVICES][64];
char classNames[MAX_PCI_DEVICES][64];

int16 vendors[MAX_PCI_DEVICES];
int16 devices[MAX_PCI_DEVICES];
int16 classes[MAX_PCI_DEVICES];
int16 subclasses[MAX_PCI_DEVICES];
int8 revisions[MAX_PCI_DEVICES];

pci_location_t pciLocations[MAX_PCI_DEVICES];

/**
 * @brief Scans (Probes) PCI Devices
 * 
 */
void probe_pci(void){
    info("Probe has been started!", __FILE__);
    int i = 0;
    for(int32 bus = 0; bus < 256; bus++)
    {
        for(int32 slot = 0; slot < 32; slot++)
        {
            for(int32 function = 0; function < 8; function++)
            {
                    int16 vendor = getVendorID((int16)bus, (int16)slot, (int16)function);
                    if(vendor == 0xffff) continue;
                    int16 device = getDeviceID((int16)bus, (int16)slot, (int16)function);
                    int16 classid = getClassId((int16)bus, (int16)slot, (int16)function);
                    int16 subclassid = getSubClassId((int16)bus, (int16)slot, (int16)function);
                    int16 revision = getRevision((int16)bus, (int16)slot, (int16)function);
                    int8 prog_if = getProgIF((int16)bus, (int16)slot, (int16)function);

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
                    pciLocations[i] = (pci_location_t){(int16)bus, (int16)slot, (int16)function};

                    vendors[i] = vendor;
                    devices[i] = device;
                    classes[i] = classid;
                    subclasses[i] = subclassid;
                    revisions[i] = (int8)revision;

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
