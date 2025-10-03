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
#include <stdbool.h>
#define READ_DMA_EXT 0x25
#define SECTOR_SIZE 512


ahci_controller* global_ahci_ctrl = null;

void detect_ahci_devices(ahci_controller* ahci_ctrl) {
    global_ahci_ctrl = ahci_ctrl;

    for (int i = 0; i < 32; i++)
    {
        ahci_port* port = &ahci_ctrl->ports[i];

        if (!port->cmd_list) {
            port->cmd_list = (ahci_command_header_t*)kmalloc(sizeof(ahci_command_header_t) * 32, 1024); // 32 headers, 1K aligned
            memset(port->cmd_list, 0, sizeof(ahci_command_header_t) * 32);
        }
    
        if (ahci_ctrl->pi & (1 << 0)) {
            port->cmd |= AHCI_PORT_CMD_ST;

            if (port->cmd & AHCI_PORT_CMD_FR) {
                port->cmd &= ~AHCI_PORT_CMD_FR;
                port->cmd |= AHCI_PORT_CMD_FRE;
            }

            while (!(port->ssts & 0x0F)) return;

            int32 sig = port->sig;
            if (sig == sata_disk) {
                printf("SATA Disk detected at port %d", i);
                uint8_t buffer[SECTOR_SIZE * 1]; // Buffer to store 5 sectors

                if (ahci_read_sectors_polling(i, 2048, 1, buffer) != 0) {
                    error("sector reading failed!", __FILE__);
                }

                if (buffer[510] == 0x55 && buffer[511] == 0xAA) {
                    debug_printf("Boot sector signature correct!\n");
                } else {
                    debug_printf("Boot sector signature invalid.\n");
                }

                for (int h = 0; h < 16; h++) {
                    debug_printf("%x ", buffer[h]);
                }
                
            } else if (sig == satapi_disk) {
                printf("SATAPI Disk detected at port %d", i);
            } else if (sig == semb_disk) {
                printf("SEMB Disk detected at port %d", i);
            } else if (sig == port_multiplier) {
                printf("Port Multiplier (PM) detected at port %d", i);
            }else{
                warn("Unknown disk detected!", __FILE__);
                printf("port->sig = 0x%x", sig);
            }
        }
    }
}

int ahci_read_sectors_polling(int port_number, uint64_t lba, uint32_t sector_count, void* buffer) {
    if (!global_ahci_ctrl) return -1;
    // cmd_table->prdt_entry[0].dba = (uint32_t)(uintptr_t)buffer;

    ahci_port* port = &global_ahci_ctrl->ports[port_number];
    if (!port) return -2;

    // Find a free command slot (0-31)
    int slot = -1;
    for (int i = 0; i < 32; i++) {
        if (!(port->ci & (1 << i))) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -3; // no free slot

    // Allocate command table in contiguous memory
    ahci_command_header_t* cmd_header = &port->cmd_list[slot];
    ahci_command_table_t* cmd_table = (ahci_command_table_t*)kmalloc(sizeof(ahci_command_table_t));
    if (!cmd_table) return -4;

    memset(cmd_table, 0, sizeof(ahci_command_table_t));
    cmd_header->ctba = (uint32_t)(uintptr_t)cmd_table;
    cmd_header->ctbau = 0; // upper 32-bit (assuming 32-bit for now)
    cmd_header->prdtl = 1; // one PRDT entry

    // Fill PRDT entry
    cmd_table->prdt_entry[0].dba = (uint32_t)(uintptr_t)buffer;   // physical address
    cmd_table->prdt_entry[0].dbau = 0;
    cmd_table->prdt_entry[0].dbc = sector_count * SECTOR_SIZE - 1;
    cmd_table->prdt_entry[0].i = 1; // interrupt on completion (ignored in polling)

    // Fill CFIS (20-byte ATA Read DMA EXT command)
    uint8_t* cfis = cmd_table->cfis;
    memset(cfis, 0, 64);
    cfis[0] = 0x27;       // FIS type: Host to device
    cfis[1] = 0x80;       // H2D, command
    cfis[2] = READ_DMA_EXT; // ATA command
    // 48-bit LBA
    cfis[4] = lba & 0xFF;
    cfis[5] = (lba >> 8) & 0xFF;
    cfis[6] = (lba >> 16) & 0xFF;
    cfis[7] = (lba >> 24) & 0xFF;
    cfis[8] = (lba >> 32) & 0xFF;
    cfis[9] = (lba >> 40) & 0xFF;
    // Sector count
    cfis[12] = sector_count & 0xFF;
    cfis[13] = (sector_count >> 8) & 0xFF;

    // Issue command
    port->ci |= (1 << slot);

    // Poll for completion
    while ((port->ci & (1 << slot)) || (port->tfd & 0x88)) {
        // optional: timeout or pause
    }

    // Check errors
    if (port->tfd & (0x1 | 0x20)) { // ERR or DF
        kfree(cmd_table);
        return -5;
    }

    // Free command table
    kfree(cmd_table);
    return 0; // success
}