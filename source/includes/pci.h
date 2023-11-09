/**
 * @file pci.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The Header files for actual PCI source
 * @version 0.1
 * @date 2023-10-29
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <hal.h>
#include <graphics.h>

extern cstring display_adapter_name;
extern cstring GPUName[1]; //Max 2 GPUs allowed

int16 pci_read_word(int16 bus, int16 slot, int16 func, int16 offset);

/**
 * @brief Gets the Vendor ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16 Vendor ID
 */
int16 getVendorID(int16 bus, int16 device, int16 function);

/**
 * @brief Gets the Device ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16  Device ID
 */
int16 getDeviceID(int16 bus, int16 device, int16 function);

/**
 * @brief Gets the Class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16 Class ID
 */
int16 getClassId(int16 bus, int16 device, int16 function);

/**
 * @brief Gets the Sub-class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return int16 Sub-class ID
 */
int16 getSubClassId(int16 bus, int16 device, int16 function);

/**
 * @brief Scans (Probes) PCI Devices
 * 
 */
void probe_pci();