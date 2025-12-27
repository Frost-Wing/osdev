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
cstring GPUName[1] = {"Frost Generic Display Driver for Graphics Processing Unit"}; // Max 2 GPUs allowed

int64* graphics_base_Address = null;
string using_graphics_card = "unknown";

typedef void (*pci_probe_fn)(
    uint8_t bus, uint8_t slot, uint8_t function
);

typedef struct {
    int16 bus;
    int16 slot;
    int16 func;
} pci_location_t;

typedef struct {
    uint16_t vendor;
    uint16_t device;
    uint8_t  classid;    // 0xFF = ignore
    cstring name;
    uint8_t is_gpu;
    pci_probe_fn probe;  // optional device-specific init
} pci_id_entry_t;

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
    int32 address = (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

/**
 * @brief Function to get the base address register (BAR) of the graphics card
 * 
 * @param bus 
 * @param slot 
 * @param func 
 * @param desiredBAR 
 * @return int64 
 */
int64 get_graphics_card_bar_address(int8 bus, int8 slot, int8 func, int8 desiredBAR) {
    for (int8 barIndex = 0; barIndex < 6; ++barIndex) {  // Assume there are 6 BARs, adjust as needed
        int32 lowerBAR = pci_config_read_dword(bus, slot, func, 0x10 + barIndex * 4);

        // Check if the BAR is memory-mapped (bit 0 set to 0) and not an IO space (bit 0 set to 1)
        if ((lowerBAR & 0x1) == 0 && (lowerBAR & 0x6) == 0) {
            // Assuming BAR is memory-mapped, so we concatenate lower and upper halves
            int32 upperBAR = pci_config_read_dword(bus, slot, func, 0x14 + barIndex * 4);
            int64 barAddress = ((int64)upperBAR << 32) | lowerBAR;

            // Check if this is the desired BAR
            if (barIndex == desiredBAR) {
                return barAddress;
            }else {
                warn("Desired BAR not found! (case 2)", __FILE__);
            }
        }
    }

    warn("Desired BAR not found! (case 1)", __FILE__);
    return 0;
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

    return pci_config_read_dword(bus, slot, func, bar_offset);
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
    int64 address;
    int64 lbus = (int64)bus;
    int64 lslot = (int64)slot;
    int64 lfunc = (int64)func;
    int16 tmp = 0;
    address = (int64)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((int32)0x80000000));
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
        return r0;
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
        return r0;
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
        return (r0 & ~0x00FF) >> 8;
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
        return (r0 & ~0xFF00);
}

/**
 * @brief Loads a int64* to a Graphics Card's base address
 * 
 * @param bus 
 * @param slot 
 * @param function 
 * @param graphics_card_name
 */
void load_graphics_card(int16 bus, int16 slot, int16 function, cstring graphics_card_name){

    for(int8 barIndex = 0; barIndex < 6; barIndex++){
        graphics_base_Address = (int64*)get_graphics_card_bar_address(bus, slot, function, barIndex);

        if(graphics_base_Address != null){
            done("Found graphics card's base address!", __FILE__);
            // printf("%d", graphics_base_Address);
            using_graphics_card = graphics_card_name;
            return;
        }else{
            warn("Cannot use graphics card. (Attempting next BAR Index)", __FILE__);
        }

    }

}

char* vendorNames[512];
char* deviceNames[512];
char* classNames[512];
pci_location_t pciLocations[512];

/**
 * @brief Scans (Probes) PCI Devices
 * 
 */
void probe_pci(){
    info("Probe has been started!", __FILE__);
    int i = 0;
    for(int32 bus = 0; bus < 256; bus++)
    {
        for(int32 slot = 0; slot < 32; slot++)
        {
            for(int32 function = 0; function < 8; function++)
            {
                    int16 vendor = getVendorID(bus, slot, function);
                    if(vendor == 0xffff) continue;
                    int16 device = getDeviceID(bus, slot, function);
                    int16 classid = getClassId(bus, slot, function);
                    int16 subclassid = getSubClassId(bus, slot, function);

                    string vendorName  = parse_vendor(vendor);
                    string className   = parse_class(classid);
                    string deviceNameBuffer[64];
                    string deviceName = deviceNameBuffer;

                    strcpy(deviceName, "Unknown Device");

                    const pci_id_entry_t* entry = pci_lookup(vendor, device, classid);

                    if (entry) {
                        deviceName = entry->name;

                        if (entry->is_gpu) {
                            display_adapter_name = deviceName;
                            GPUName[gpu_index++] = deviceName;
                        }

                        if (entry->probe)
                            entry->probe(bus, slot, function);
                    } else if (classid == 0x03) {
                        deviceName = auto_name_gpu(vendor, device);
                        display_adapter_name = deviceName;
                        GPUName[gpu_index++] = deviceName;
                    }

                    if (classid == 0x03) {
                        if (vendor == 0x1b36 && device == 0x0100)
                            warn("QXL graphics not supported.", __FILE__);
                        else
                            load_graphics_card(bus, slot, function, deviceName);
                    }

                    
                    if (strcmp(deviceName, "Unknown Device") == 0) {
                        snprintf(deviceName, sizeof(deviceNameBuffer), "Unknown Device (0x%04X)", device);
                    }

                    
                    print(green_color);
                    printf("%9s : Device : %9s -- Class : %9s", vendorName, deviceName, className);
                    print(reset_color);
                    
                    debug_printf(green_color);
                    debug_printf("%s : Device : %s -- Class : %s\n", vendorName, deviceName, className);
                    debug_printf(reset_color);
                    
                    vendorNames[i] = vendorName;
                    deviceNames[i] = deviceName;
                    classNames[i]  = className;
                    pciLocations[i] = (pci_location_t){bus, slot, function};

                    vendorName = "";
                    deviceName = "";
                    className  = "";
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

void print_lspci() {
    for (int i = 0; i < total_devices; i++) {
        if (vendorNames[i] == NULL || deviceNames[i] == NULL || classNames[i] == NULL)
            continue; // Skip empty entries

            printf("%02d:%2x.%d " yellow_color "%s " green_color "%s " red_color "%s" reset_color,
                pciLocations[i].bus,
                pciLocations[i].slot, 
                pciLocations[i].func,
                vendorNames[i],
                deviceNames[i],
                classNames[i]);
    }
}