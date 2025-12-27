/**
 * @file pci_id.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The list of all officially recognized devices.
 * @version 0.1
 * @date 2025-12-27
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#ifndef PCI_ID_H
#define PCI_ID_H

#include <basics.h>
#include <pci_id.h>

typedef void (*pci_probe_fn)(uint8_t bus, uint8_t slot, uint8_t function);

typedef struct {
    uint16_t vendor;
    uint16_t device;
    uint8_t  classid;    // 0xFF = ignore
    const char* name;
    uint8_t is_gpu;
    pci_probe_fn probe;  // optional device-specific init
} pci_id_entry_t;

/**
 * @brief Gets the vendor id and gives the appropriate name.
 * 
 * @param vendor vendor id
 * @return string vendor name
 */
string parse_vendor(int16 vendor);

/**
 * @brief Gets the class id and gives the appropriate class name.
 * 
 * @param classid class id
 * @return string class name
 */
string parse_class(int16 classid);

/**
 * @brief A Simplified and clean PCI lookup function.
 * 
 * @param vendor The vendor id.
 * @param device The device id.
 * @param classid The class id.
 * @return const pci_id_entry_t*;s
 */
const pci_id_entry_t* pci_lookup(uint16_t vendor, uint16_t device, uint8_t classid);

/**
 * @brief Finds the AHCI BAR and handles it.
 * 
 * @param bus 
 * @param slot 
 * @param function 
 */
void probe_ahci(uint8_t bus, uint8_t slot, uint8_t function);

/**
 * @brief Finds the RTL8139 card and handles it.
 * 
 * @param bus 
 * @param slot 
 * @param function 
 */
void probe_rtl8139(uint8_t bus, uint8_t slot, uint8_t function);

/**
 * @brief Automatically names the unknown GPU with vendor name and its device id.
 * 
 * @param vendor the vendor id.
 * @param device devide id.
 * @return cstring; gpu name.
 */
cstring auto_name_gpu(uint16_t vendor, uint16_t device);

#endif