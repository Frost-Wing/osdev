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
    uint16_t cfl:5;      // Command FIS length in DWORDS
    uint16_t a:1;        // ATAPI
    uint16_t w:1;        // Write (1 = H->D, 0 = D->H)
    uint16_t p:1;        // Prefetchable
    uint16_t r:1;        // Reset
    uint16_t b:1;        // BIST
    uint16_t c:1;        // Clear busy upon R_OK
    uint16_t rsv0:1;     // Reserved
    uint16_t pmp:4;      // Port multiplier port
    uint16_t prdtl;      // Number of PRDT entries
    uint32_t prdbc;      // Physical region descriptor byte count transferred
    uint32_t ctba;       // Command table base address (lower 32-bit)
    uint32_t ctbau;      // Command table base address upper 32-bit
    uint32_t rsv1[4];    // Reserved
} __attribute__((packed)) ahci_command_header_t;


// Structure to represent a Physical Region Descriptor
typedef struct {
    uint32_t dba;      // Data base address
    uint32_t dbau;     // Data base address upper 32 bits
    uint32_t rsv0;     // Reserved
    uint32_t dbc:22;   // Byte count, 4M max
    uint32_t rsv1:9;   
    uint32_t i:1;      // Interrupt on completion
} __attribute__((packed)) prdt_entry_t;

typedef struct {
    uint8_t cfis[64];          // Command FIS
    uint8_t acmd[16];          // ATAPI command (optional)
    uint8_t rsv[48];           // Reserved
    prdt_entry_t prdt_entry[1]; // PRDT entries (can allocate more)
} __attribute__((packed)) ahci_command_table_t;

typedef volatile struct {
    int32 clb;        // 0x00, command list base address, 1K-byte aligned
    int32 clbu;       // 0x04, command list base address upper 32 bits
    int32 fb;         // 0x08, FIS base address, 256-byte aligned
    int32 fbu;        // 0x0C, FIS base address upper 32 bits
    int32 is;         // 0x10, interrupt status
    int32 ie;         // 0x14, interrupt enable
    int32 cmd;        // 0x18, command and status
    int32 rsv0;       // 0x1C, reserved
    int32 tfd;        // 0x20, task file data
    int32 sig;        // 0x24, signature
    int32 ssts;       // 0x28, SATA status (SCR0:SStatus)
    int32 sctl;       // 0x2C, SATA control (SCR2:SControl)
    int32 serr;       // 0x30, SATA error (SCR1:SError)
    int32 sact;       // 0x34, SATA active (SCR3:SActive)
    int32 ci;         // 0x38, command issue
    int32 sntf;       // 0x3C, SATA notification (SCR4:SNotification)
    int32 fbs;        // 0x40, FIS-based switch control
    int32 rsv1[11];   // 0x44 ~ 0x6F, reserved
    int32 vendor[4];  // 0x70 ~ 0x7F, vendor specific
    ahci_command_header_t* cmd_list; // pointer to command header array
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


int ahci_read_sectors_polling(int port_number, uint64_t lba, uint32_t sector_count, void* buffer);