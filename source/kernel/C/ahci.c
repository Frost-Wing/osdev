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

ahci_controller* global_ahci_ctrl = null;

void detect_ahci_devices(ahci_controller* ahci_ctrl) {
    global_ahci_ctrl = ahci_ctrl;

    for (int i = 0; i < 32; i++)
    {
        ahci_port* port = &ahci_ctrl->ports[i];
    
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
                uint8_t sector_buffer[SECTOR_SIZE * 1]; // Buffer to store 5 sectors

                if (read_sectors(0, 1, sector_buffer) != 0) {
                    error("sector reading failed!", __FILE__);
                }

                for(int x = 0; x < SECTOR_SIZE * 1; x++){
                    debug_printf("%u ", sector_buffer[x]);
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

ahci_command_header_t* get_free_command_header() {
    // Iterate through command headers to find a free one
    for (int i = 0; i < 32; i++) {
        if (!(global_ahci_ctrl->ports[0].ci & 1)) { 
            info("FOUND FREE CMD HEADER!", __FILE__);
            printf("Command header = %d", i);
            return &global_ahci_ctrl->ports[0].cmd; 
        }
    }
    // No free command headers available
    return NULL;
}

// Function to allocate memory for PRDT entries
prdt_entry_t* allocate_prdt(size_t num_entries) {
    // Allocate memory for the desired number of PRDT entries
    return (prdt_entry_t*)malloc(num_entries * sizeof(prdt_entry_t)); 
}

int read_sectors(uint32_t lba, uint32_t sector_count, void* buffer) {
    // 1. Get AHCI command header and PRDT
    ahci_command_header_t* cmd_header = get_free_command_header();
    prdt_entry_t* prdt = allocate_prdt(1);

    // 2. Fill command FIS
    cmd_header->cfis[0] = 0x27; // ATA command: Read
    cmd_header->cfis[2] = lba & 0xFF;
    cmd_header->cfis[3] = (lba >> 8) & 0xFF;
    cmd_header->cfis[4] = (lba >> 16) & 0xFF;
    cmd_header->cfis[5] = (lba >> 24) & 0xFF;
    cmd_header->cfis[7] = sector_count & 0xFF;
    cmd_header->cfis[8] = (sector_count >> 8) & 0xFF;

    // 3. Fill PRDT
    prdt[0].dba = (uint32_t)buffer; // Address of the buffer to store data
    prdt[0].dbc = sector_count * SECTOR_SIZE - 1; // Number of bytes to transfer

    // 4. Set up command header
    cmd_header->prdtl = sizeof(prdt_entry_t); // Size of PRDT
    cmd_header->prdt = (uint32_t)prdt;

    // 5. Issue the command
    cmd_header->ci = 1; // Issue command


    sleep(2);

    // 7. Read data from the buffer

    return 0; // Command successful
}