/**
 * @file ahci.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief AHCI Drivers for FrostWing OS
 * @version 0.1
 * @date 2023-12-15
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <ahci.h>
#include <heap.h>

ahci_hba_mem_t* global_ahci_ctrl;

void detect_ahci_devices(ahci_hba_mem_t* ahci_ctrl) {
    global_ahci_ctrl = ahci_ctrl;

    uint32_t ports_implemented = (uint32_t)(ahci_ctrl->pi);

    for (int i = 0; i < 32; i++) {
        if (!(ports_implemented & (1 << i)))
            continue;

        ahci_port_t* port = &ahci_ctrl->ports[i];
        uint32_t ssts = port->ssts;

        uint8_t det = (uint8_t)(ssts & 0x0F);         // device detection
        uint8_t ipm = (uint8_t)((ssts >> 8) & 0x0F);  // interface power management

        if (det != AHCI_PORT_DET_PRESENT || ipm != AHCI_PORT_IPM_ACTIVE)
            continue;

        uint32_t sig = port->sig;

        switch (sig) {
            case sata_disk:
                printf("[AHCI] SATA Disk detected on port %d", i);

                void* sector_buf = kmalloc_aligned(SECTOR_SIZE, 512);
                if (!sector_buf) {
                    error("Memory allocation failed for sector buffer", __FILE__);
                    break;
                }

                // Try reading LBA 0 (the first sector)
                int status = ahci_read_sector(i, 0, sector_buf, 1);

                if (status == 0) {
                    printf("[AHCI] Successfully read sector 0 on port %d", i);

                    // Print first 16 bytes as hex
                    uint8_t* data = (uint8_t*)sector_buf;
                    printf("Sector 0 data: ");
                    for (int b = 0; b < 16; b++)
                        printfnoln("%c ", data[b]);

                    printf("...\n");
                } else {
                    printf("[AHCI] Failed to read sector on port %d (status=%d)", i, status);
                }
                break;
            case satapi_disk:
                printf("[AHCI] SATAPI device detected on port %d", i);
                break;
            case semb_disk:
                printf("[AHCI] SEMB device detected on port %d", i);
                break;
            case port_multiplier:
                printf("[AHCI] Port Multiplier detected on port %d", i);
                break;
            default:
                printf("[AHCI] Unknown device (sig=0x%X) on port %d", sig, i);
                break;
        }
    }
}

/**
 * @brief Read sectors via AHCI (LBA48 READ_DMA_EXT)
 */
int ahci_read_sector(int port_number, uint64_t lba, void* buffer, uint32_t sector_count) {
    if (!global_ahci_ctrl)
        return -1;

    ahci_port_t* port = &global_ahci_ctrl->ports[port_number];

    // Stop command engine
    port->cmd &= ~AHCI_PORT_CMD_ST;
    while (port->cmd & AHCI_PORT_CMD_CR);

    // Allocate command list (1K aligned)
    ahci_cmd_header_t* cmd_list = kmalloc_aligned(sizeof(ahci_cmd_header_t) * 32, 1024);
    memset(cmd_list, 0, sizeof(ahci_cmd_header_t) * 32);
    port->clb = (uint32_t)(uintptr_t)cmd_list;
    port->clbu = 0;

    // Allocate FIS receive buffer (256-byte aligned)
    void* fis_base = kmalloc_aligned(256, 256);
    memset(fis_base, 0, 256);
    port->fb = (uint32_t)(uintptr_t)fis_base;
    port->fbu = 0;

    // Clear interrupts and errors
    port->is = 0xFFFFFFFF;
    port->serr = 0xFFFFFFFF;

    // Start command engine
    port->cmd |= (AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST);

    // Find a free command slot
    int slot = -1;
    for (int i = 0; i < 32; i++) {
        if (!(port->ci & (1 << i))) {
            slot = i;
            break;
        }
    }
    if (slot == -1)
        return -2; // no free slot

    ahci_cmd_header_t* header = &cmd_list[slot];

    // Allocate command table
    ahci_cmd_table_t* table = kmalloc_aligned(sizeof(ahci_cmd_table_t), 128);
    memset(table, 0, sizeof(ahci_cmd_table_t));

    header->flags = (sizeof(table->cfis) / 4) & AHCI_CMD_HDR_CFL_MASK;
    header->prdtl = 1;
    header->ctba = (uint32_t)(uintptr_t)table;
    header->ctbau = 0;

    // Setup PRDT
    table->prdt[0].dba  = (uint32_t)(uintptr_t)buffer;
    table->prdt[0].dbau = 0;
    table->prdt[0].dbc  = (sector_count * SECTOR_SIZE - 1) | (1 << 31); // set interrupt bit

    // Setup command FIS
    uint8_t* cfis = table->cfis;
    memset(cfis, 0, 64);
    cfis[0] = 0x27;  // Host to device FIS
    cfis[1] = 0x80;  // Command FIS
    cfis[2] = READ_DMA_EXT;
    cfis[4] = (uint8_t)(lba & 0xFF);
    cfis[5] = (uint8_t)((lba >> 8) & 0xFF);
    cfis[6] = (uint8_t)((lba >> 16) & 0xFF);
    cfis[7] = (uint8_t)((lba >> 24) & 0xFF);
    cfis[8] = (uint8_t)((lba >> 32) & 0xFF);
    cfis[9] = (uint8_t)((lba >> 40) & 0xFF);
    cfis[12] = (uint8_t)(sector_count & 0xFF);
    cfis[13] = (uint8_t)((sector_count >> 8) & 0xFF);

    // Issue command
    port->ci |= (1 << slot);

    // Wait for completion
    uint64_t timeout = 1000000;
    while ((port->ci & (1 << slot)) && --timeout);

    if (timeout == 0 || (port->tfd & (0x1 | 0x20)))
        return -3; // timeout or error

    return 0; // success
}