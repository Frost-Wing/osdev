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
#include <graphics.h>

#define AHCI_PORT_DET_PRESENT 3
#define AHCI_PORT_IPM_ACTIVE 1
#define AHCI_PORT_CMD_ST    (1 << 0)
#define AHCI_PORT_CMD_FRE   (1 << 4)
#define AHCI_PORT_CMD_FR    (1 << 14)
#define AHCI_PORT_CMD_CR    (1 << 15)
#define READ_DMA_EXT        0x25
#define SECTOR_SIZE         512

#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_IDENTIFY 0xEC

#define MAX_PARTITIONS 128


/**
 * @brief AHCI device signatures.
 */
#define sata_disk         0x00000101
#define satapi_disk       0xEB140101
#define semb_disk         0xC33C0101
#define port_multiplier   0x96690101

/**
 * @brief AHCI command header structure.
 *
 * Represents the command header for a single AHCI port.
 */
typedef struct __attribute__((packed)) {
    /* first 16-bit flags */
    uint16_t flags;      /* contains CFL(5), A, W, P, R, B, C, rsv0, PMP(4) */
    uint16_t prdtl;      /* PRDT length (entries) */
    uint32_t prdbc;      /* PRDT byte count transferred (updated by HBA) */
    uint32_t ctba;       /* Command table descriptor base address (lower 32) */
    uint32_t ctbau;      /* Command table descriptor base address upper 32 */
    uint32_t rsv[4];
} ahci_cmd_header_t;

#define AHCI_CMD_HDR_CFL_MASK   0x001F  /* bits 0-4 */
#define AHCI_CMD_HDR_A_BIT      0x0020
#define AHCI_CMD_HDR_W_BIT      0x0040
#define AHCI_CMD_HDR_P_BIT      0x0080
#define AHCI_CMD_HDR_R_BIT      0x0100
#define AHCI_CMD_HDR_B_BIT      0x0200
#define AHCI_CMD_HDR_C_BIT      0x0400
#define AHCI_CMD_HDR_PMP_MASK   0xF000  /* bits 12-15 */

/**
 * @brief Physical Region Descriptor Table (PRDT) entry.
 */
 typedef struct __attribute__((packed)) {
    uint32_t dba;    /* Data base address (lower 32) */
    uint32_t dbau;   /* Data base address upper 32 (if S64A supported) */
    uint32_t reserved;
    uint32_t dbc;    /* DW3: byte count and flags (use macros) */
} prdt_entry_t;


/**
 * @brief AHCI command table structure.
 */
 typedef struct __attribute__((packed)) {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    prdt_entry_t prdt[1]; /* flexible / variable sized in allocation */
} ahci_cmd_table_t;

typedef struct {
    ahci_cmd_header_t* cmd_list;
    ahci_cmd_table_t*  cmd_tables[32];
    void*              fis;
} ahci_port_mem_t;

static ahci_port_mem_t port_mem[32];


/**
 * @brief AHCI port registers and command header pointer.
 */
 typedef volatile struct {
    uint32_t clb;        /* 0x00 Command list base address (lower 32) */
    uint32_t clbu;       /* 0x04 Command list base address upper 32 */
    uint32_t fb;         /* 0x08 FIS base address (lower 32) */
    uint32_t fbu;        /* 0x0C FIS base address upper 32 */
    uint32_t is;         /* 0x10 Interrupt status */
    uint32_t ie;         /* 0x14 Interrupt enable */
    uint32_t cmd;        /* 0x18 Command and status */
    uint32_t rsv0;       /* 0x1C Reserved */
    uint32_t tfd;        /* 0x20 Task file data */
    uint32_t sig;        /* 0x24 Signature */
    uint32_t ssts;       /* 0x28 SATA status (SStatus) */
    uint32_t sctl;       /* 0x2C SATA control (SControl) */
    uint32_t serr;       /* 0x30 SATA error (SError) */
    uint32_t sact;       /* 0x34 SATA active (SActive) */
    uint32_t ci;         /* 0x38 Command issue */
    uint32_t sntf;       /* 0x3C SATA notification (SNotification) */
    uint32_t fbs;        /* 0x40 FIS-based switching */
    uint32_t rsv1[11];   /* 0x44 - 0x6F reserved */
    uint32_t vendor[4];  /* 0x70 - 0x7F vendor specific */
} ahci_port_t;

typedef volatile struct {
    uint32_t cap;       /* 0x00 Host capabilities (CAP) */
    uint32_t ghc;       /* 0x04 Global host control (GHC) */
    uint32_t is;        /* 0x08 Interrupt status */
    uint32_t pi;        /* 0x0C Ports implemented */
    uint32_t vs;        /* 0x10 Version */
    uint32_t ccc_ctl;   /* 0x14 Command completion coalescing control */
    uint32_t ccc_pts;   /* 0x18 Command completion ports */
    uint32_t em_loc;    /* 0x1C Enclosure management location */
    uint32_t em_ctl;    /* 0x20 Enclosure management control */
    uint32_t cap2;      /* 0x24 Host capabilities extended */
    uint32_t bohc;      /* 0x28 BIOS/OS handoff control and status */
    uint8_t  rsv[0xA0 - 0x2C];   /* reserved bytes up to vendor area */
    uint8_t  vendor[0x100 - 0xA0];/* vendor specific area */
    /* 0x100 - start of port control registers (ports[0] starts here) */
    ahci_port_t ports[32];       /* array of port structures (0x100.. ) */
} ahci_hba_mem_t;

typedef struct {
    uint64_t total_sectors;
    int present;
} ahci_disk_info_t;

/// GENERAL PARITION LAYOUT BEGIN

typedef enum {
    FS_UNKNOWN = 0,

    // FAT family
    FS_FAT12,
    FS_FAT16,
    FS_FAT32,
    FS_EXFAT,

    // Linux / Unix
    FS_EXT2,
    FS_EXT3,
    FS_EXT4,
    FS_XFS,
    FS_BTRFS,

    // Other common FS
    FS_NTFS,
    FS_ISO9660,
    FS_UDF,

    // OS / Custom
    FS_FROSTFS,
    FS_RAMFS,
} partition_fs_type_t;


typedef enum {
    PART_TABLE_MBR,
    PART_TABLE_GPT
} partition_table_type_t;

typedef struct {
    partition_table_type_t table_type;

    // Common geometry
    int64 lba_start;
    int64 lba_end;
    int64 sector_count;
    int64 ahci_port;

    // Bootable?
    bool bootable;

    // Filesystem info
    partition_fs_type_t fs_type;

    // Original on-disk identifiers (optional but useful)
    union {
        uint8_t  mbr_type;          // raw MBR partition type
        uint8_t  gpt_type_guid[16]; // raw GPT type GUID
    };

    char name[64]; // UTF-8, converted from GPT UTF-16 if needed
} general_partition_t;

typedef struct mount_entry {
    char* mount_point;
    char* part_name;
    partition_fs_type_t type;
    void* fs;
} mount_entry_t;

// END

extern ahci_disk_info_t ahci_disks[32];

extern general_partition_t ahci_partitions[MAX_PARTITIONS];
extern mount_entry_t mounted_partitions[MAX_PARTITIONS];
extern int general_partition_count;
extern int mounted_partiion_count;

/**
 * @brief Global AHCI controller pointer.
 */
extern ahci_hba_mem_t* global_ahci_ctrl;

/**
 * @brief Probes and detects all AHCI devices connected to the controller.
 * 
 * @param ahci_ctrl Pointer to the AHCI controller structure.
 */
void detect_ahci_devices(ahci_hba_mem_t* ahci_ctrl);

general_partition_t* add_general_partition(
    partition_table_type_t table_type,
    int64 lba_start,
    int64 lba_end,
    int64 sector_count,
    int64 ahci_port,
    bool bootable,
    partition_fs_type_t fs_type,
    cstring name,
    uint8_t mbr_type,
    const uint8_t* gpt_guid   // must be 16 bytes
);

general_partition_t* search_general_partition(cstring partition_name);

mount_entry_t* add_mount(const char* mount_point, const char* part_name, partition_fs_type_t type, void* fs_ptr);

#endif // AHCI_H
