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
#include <drivers/rtl8139.h>
#include <debugger.h>

extern cstring display_adapter_name;
extern cstring GPUName[1]; //Max 2 GPUs allowed

extern string using_graphics_card;

extern int64* graphics_base_Address;

// Define the base address for the PCI configuration space
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

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

/**
 * @brief Loads a int64* to a Graphics Card's base address
 * 
 * @param bus 
 * @param slot 
 * @param function 
 * @param graphics_card_name
 */
void load_graphics_card(int16 bus, int16 slot, int16 function, cstring graphics_card_name);

/**
 * @brief Function to get the base address register (BAR) of the graphics card
 * 
 * @param bus 
 * @param slot 
 * @param func 
 * @param desiredBAR 
 * @return int64 
 */
int64 get_graphics_card_bar_address(int8 bus, int8 slot, int8 func, int8 desiredBAR);

/**
 * @brief Function to read a 32-bit value from the PCI configuration space
 * 
 * @param bus 
 * @param slot 
 * @param func 
 * @param offset 
 * @return int32 
 */
int32 pci_config_read_dword(int8 bus, int8 slot, int8 func, int8 offset);

/**
 * @brief Gets the AHCI bar address
 * 
 * @param bus The PCI bus number.
 * @param slot The PCI slot number.
 * @param func The PCI function number.
 * @param bar_num The Base Address Register Number.
 * @return int32 
 */
int32 get_ahci_bar_address(int8 bus, int slot, int func, int bar_num);