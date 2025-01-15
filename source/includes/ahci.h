/**
 * @file ahci.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The headers for AHCI Driver.
 * @version 0.1
 * @date 2023-12-16
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>

#define AHCI_PORT_BASE    0x400

#define AHCI_PORT_CMD_ST  0x1
#define AHCI_PORT_CMD_FRE 0x10
#define AHCI_PORT_CMD_FR  0x400

#define sata_disk         0x00000101
#define satapi_disk       0xEB140101
#define semb_disk         0xC33C0101
#define port_multiplier   0x96690101

#define SECTOR_SIZE 512

// Structure to represent an AHCI command header
typedef struct {
    uint32_t cfl;       // Command FIS length
    uint8_t cfis[64];    // Command FIS
    uint32_t prdtl;      // Physical Region Descriptor Table Length
    uint32_t prdt;      // Physical Region Descriptor Table
    uint32_t reserved[4];
    uint32_t ciss;      // Command and Status
    uint32_t iss;       // Interrupt Status
    uint32_t sact;      // Start/Activate
    uint32_t ci;        // Command Issue
    uint32_t dsts;      // Data Strobe
    uint32_t serr;      // Serialization Error
    uint32_t intm;      // Interrupt Mask
    uint32_t cmd;       // Command
    uint32_t fis;       // FIS register
    uint32_t ctl;       // Control register
} __attribute__((packed)) ahci_command_header_t;

// Structure to represent a Physical Region Descriptor
typedef struct {
    uint32_t dba;     // Device Base Address
    uint32_t dbc;     // Data Base Count
} __attribute__((packed)) prdt_entry_t;

typedef volatile struct {
    int32 clb;         // Command List Base Address, 1K-byte aligned
    int32 clbu;        // Command List Base Address Upper 32 Bits
    int32 fb;          // FIS Base Address, 256-byte aligned
    int32 fbu;         // FIS Base Address Upper 32 Bits
    int32 is;          // Interrupt Status
    int32 ie;          // Interrupt Enable
    int32 cmd;         // Command and Status
    int32 rsv0;        // Reserved
    int32 tfd;         // Task File Data
    int32 sig;         // Signature
    int32 ssts;        // Serial ATA Status (SCR0:SStatus)
    int32 sctl;        // Serial ATA Control (SCR2:SControl)
    int32 serr;        // Serial ATA Error (SCR1:SError)
    int32 sact;        // Serial ATA Active (SCR3:SActive)
    int32 ci;          // Command Issue
    int32 sntf;        // SATA Notification Register
    int32 fbs;         // FIS-based Switching Control
    int32 rsv1[11];    // Reserved
    int32 vendor[4];   // Vendor specific
} ahci_port;

typedef volatile struct {
    int32 cap;         // Host Capabilities
    int32 ghc;         // Global Host Control
    int32 is;          // Interrupt Status
    int32 pi;          // Port Implemented
    int32 vs;          // Version
    int32 ccc_ctl;     // Command Completion Coalescing Control
    int32 ccc_pts;     // Command Completion Coalescing Ports
    int32 em_loc;      // Enclosure Management Location
    int32 em_ctl;      // Enclosure Management Control
    int32 cap2;        // Host Capabilities Extended
    int32 bohc;        // BIOS/OS Handoff Control and Status
    int8  rsv[0xA0-0x2C];
    int8  vendor[0x100-0xA0];
    ahci_port ports[32];   // Port control registers
} ahci_controller;

extern ahci_controller* global_ahci_ctrl;

/**
 * @brief Probes and detects all the AHCI Devices
 * 
 * @param ahci_ctrl The pointer to the ahci_controller structure or BAR of AHCI
 */
void detect_ahci_devices(ahci_controller* ahci_ctrl);