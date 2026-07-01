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

#ifndef PCI_H
#define PCI_H

#include <basics.h>
#include <hal.h>
#include <graphics.h>
#include <drivers/rtl8139.h>
#include <debugger.h>
#include <ahci.h>
#include <nvme.h>
#include <hal.h>
#include <isr.h>
#include <pci_id.h>

extern cstring display_adapter_name;
extern cstring GPUName[2]; //Max 2 GPUs allowed
extern cstring using_graphics_card;
extern uint64* graphics_base_Address;
extern int total_devices;

// Define the base address for the PCI configuration space
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

typedef void (*pci_probe_fn)(uint8_t bus, uint8_t slot, uint8_t function);

typedef struct {
    uint16 bus;
    uint16 slot;
    uint16 func;
} pci_location_t;

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
uint16 pci_read_word(uint16 bus, uint16 slot, uint16 func, uint16 offset);

/**
 * @brief Gets the Vendor ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16 Vendor ID
 */
uint16 getVendorID(uint16 bus, uint16 device, uint16 function);

/**
 * @brief Gets the Device ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16  Device ID
 */
uint16 getDeviceID(uint16 bus, uint16 device, uint16 function);

/**
 * @brief Gets the Class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16 Class ID
 */
uint16 getClassId(uint16 bus, uint16 device, uint16 function);

/**
 * @brief Gets the Sub-class ID from PCI
 * 
 * @param bus 
 * @param device 
 * @param function 
 * @return uint16 Sub-class ID
 */
uint16 getSubClassId(uint16 bus, uint16 device, uint16 function);
uint8 getRevision(uint16 bus, uint16 slot, uint16 func);
uint8 getProgIF(uint16 bus, uint16 device, uint16 function);

/**
 * @brief Scans (Probes) PCI Devices
 * 
 */
void probe_pci(void);
void print_lspci(void);

/**
 * @brief Function to read a 32-bit value from the PCI configuration space
 * 
 * @param bus 
 * @param slot 
 * @param func 
 * @param offset 
 * @return uint32 
 */
uint32 pci_config_read_dword(uint8 bus, uint8 slot, uint8 func, uint8 offset);
void pci_config_write_dword(uint8 bus, uint8 slot, uint8 func, uint8 offset, uint32_t value);

/**
 * @brief Gets the AHCI bar address
 * 
 * @param bus The PCI bus number.
 * @param slot The PCI slot number.
 * @param func The PCI function number.
 * @param bar_num The Base Address Register Number.
 * @return uint32 
 */
uint32 get_ahci_bar_address(uint8 bus, int slot, int func, int bar_num);

/**
 * @brief Registers the PCI devices into the procfs.
 * 
 * @return int 0 always
 */
int proc_pci_register();

#endif
