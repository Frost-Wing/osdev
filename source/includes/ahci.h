/**
 * @file ahci.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Header definitions and structures for the AHCI Driver.
 * @version 0.1
 * @date 2023-12-16
 * 
 * @copyright Copyright (c) Pradosh 2023
 */

#ifndef AHCI_H
#define AHCI_H

#include <basics.h>

/**
 * @brief Base memory address for AHCI ports.
 */
#define AHCI_PORT_BASE    0x400

/**
 * @brief AHCI port command flags.
 */
#define AHCI_PORT_CMD_ST  0x1       /**< Start command. */
#define AHCI_PORT_CMD_FRE 0x10      /**< FIS receive enable. */
#define AHCI_PORT_CMD_FR  0x400     /**< FIS receive running. */

/**
 * @brief AHCI device signatures.
 */
#define sata_disk         0x00000101
#define satapi_disk       0xEB140101
#define semb_disk         0xC33C0101
#define port_multiplier   0x96690101

/**
 * @brief Standard sector size in bytes.
 */
#define SECTOR_SIZE 512

/**
 * @brief AHCI command header structure.
 *
 * Represents the command header for a single AHCI port.
 */
typedef struct {
    uint16_t cfl:5;      /**< Command FIS length in DWORDS */
    uint16_t a:1;        /**< ATAPI flag */
    uint16_t w:1;        /**< Write flag (1 = Host to Device, 0 = Device to Host) */
    uint16_t p:1;        /**< Prefetchable */
    uint16_t r:1;        /**< Reset */
    uint16_t b:1;        /**< BIST flag */
    uint16_t c:1;        /**< Clear busy upon R_OK */
    uint16_t rsv0:1;     /**< Reserved */
    uint16_t pmp:4;      /**< Port multiplier port */
    uint16_t prdtl;      /**< Number of PRDT entries */
    uint32_t prdbc;      /**< Physical Region Descriptor byte count transferred */
    uint32_t ctba;       /**< Command table base address (lower 32 bits) */
    uint32_t ctbau;      /**< Command table base address upper 32 bits */
    uint32_t rsv1[4];    /**< Reserved */
} __attribute__((packed)) ahci_command_header_t;

/**
 * @brief Physical Region Descriptor Table (PRDT) entry.
 */
typedef struct {
    uint32_t dba;        /**< Data base address */
    uint32_t dbau;       /**< Data base address upper 32 bits */
    uint32_t rsv0;       /**< Reserved */
    uint32_t dbc:22;     /**< Byte count (maximum 4M) */
    uint32_t rsv1:9;     
    uint32_t i:1;        /**< Interrupt on completion */
} __attribute__((packed)) prdt_entry_t;

/**
 * @brief AHCI command table structure.
 */
typedef struct {
    uint8_t cfis[64];            /**< Command FIS */
    uint8_t acmd[16];            /**< ATAPI command (optional) */
    uint8_t rsv[48];             /**< Reserved */
    prdt_entry_t prdt_entry[1];  /**< PRDT entries (can allocate more as needed) */
} __attribute__((packed)) ahci_command_table_t;

/**
 * @brief AHCI port registers and command header pointer.
 */
typedef volatile struct {
    int32 clb;        /**< Command list base address (1KB aligned) */
    int32 clbu;       /**< Command list base address upper 32 bits */
    int32 fb;         /**< FIS base address (256-byte aligned) */
    int32 fbu;        /**< FIS base address upper 32 bits */
    int32 is;         /**< Interrupt status */
    int32 ie;         /**< Interrupt enable */
    int32 cmd;        /**< Command and status */
    int32 rsv0;       /**< Reserved */
    int32 tfd;        /**< Task file data */
    int32 sig;        /**< Signature */
    int32 ssts;       /**< SATA status (SCR0: SStatus) */
    int32 sctl;       /**< SATA control (SCR2: SControl) */
    int32 serr;       /**< SATA error (SCR1: SError) */
    int32 sact;       /**< SATA active (SCR3: SActive) */
    int32 ci;         /**< Command issue */
    int32 sntf;       /**< SATA notification (SCR4: SNotification) */
    int32 fbs;        /**< FIS-based switch control */
    int32 rsv1[11];   /**< Reserved */
    int32 vendor[4];  /**< Vendor specific */
    ahci_command_header_t* cmd_list; /**< Pointer to command header array */
} ahci_port;

/**
 * @brief AHCI controller registers.
 */
typedef volatile struct {
    int32 cap;         /**< Host Capabilities */
    int32 ghc;         /**< Global Host Control */
    int32 is;          /**< Interrupt Status */
    int32 pi;          /**< Port Implemented */
    int32 vs;          /**< Version */
    int32 ccc_ctl;     /**< Command Completion Coalescing Control */
    int32 ccc_pts;     /**< Command Completion Coalescing Ports */
    int32 em_loc;      /**< Enclosure Management Location */
    int32 em_ctl;      /**< Enclosure Management Control */
    int32 cap2;        /**< Host Capabilities Extended */
    int32 bohc;        /**< BIOS/OS Handoff Control and Status */
    int8  rsv[0xA0-0x2C]; /**< Reserved */
    int8  vendor[0x100-0xA0]; /**< Vendor specific */
    ahci_port ports[32]; /**< AHCI port control registers */
} ahci_controller;

/**
 * @brief Global AHCI controller pointer.
 */
extern ahci_controller* global_ahci_ctrl;

/**
 * @brief Probes and detects all AHCI devices connected to the controller.
 * 
 * @param ahci_ctrl Pointer to the AHCI controller structure.
 */
void detect_ahci_devices(ahci_controller* ahci_ctrl);

/**
 * @brief Reads sectors from a port using polling mode.
 * 
 * @param port_number The AHCI port number to read from.
 * @param lba Logical block address to start reading.
 * @param sector_count Number of sectors to read.
 * @param buffer Pointer to the buffer to store data.
 * @return int 0 on success, negative on failure.
 */
int ahci_read_sectors_polling(int port_number, uint64_t lba, uint32_t sector_count, void* buffer);

#endif // AHCI_H
