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

extern const char* display_adapter_name;
extern const char* GPUName[1]; //Max 2 GPUs allowed

uint16_t pci_read_word(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset);

/**
 * @brief Gets the Vendor ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16_t Vendor ID
 */
uint16_t getVendorID(uint16_t bus, uint16_t device, uint16_t function);

/**
 * @brief Gets the Device ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16_t  Device ID
 */
uint16_t getDeviceID(uint16_t bus, uint16_t device, uint16_t function);

/**
 * @brief Gets the Class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16_t Class ID
 */
uint16_t getClassId(uint16_t bus, uint16_t device, uint16_t function);

/**
 * @brief Gets the Sub-class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16_t Sub-class ID
 */
uint16_t getSubClassId(uint16_t bus, uint16_t device, uint16_t function);

/**
 * @brief Scans (Probes) PCI Devices
 * 
 */
void probe_pci();