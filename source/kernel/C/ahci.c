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

                if (read_sectors_broken(1, 1, sector_buffer) != 0) {
                    error("sector reading failed!", __FILE__);
                }

                // for(int x = 0; x < SECTOR_SIZE * 1; x++){
                //     debug_printf("%u ", (int)sector_buffer[x]);
                // }
                // debug_print("\n");

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
            return (ahci_command_header_t*)&global_ahci_ctrl->ports[0].cmd; 
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

prdt_entry_t* free_prdt(prdt_entry_t* a) {
    return (prdt_entry_t*)free(a); 
}

int read_sectors_broken(uint32_t lba, uint32_t sector_count, void* buffer) {
    if (sector_count == 0) return -1; // Handle invalid input

    ahci_command_header_t* cmd_header = get_free_command_header();
    if (!cmd_header) return -1; // Handle allocation failure

    prdt_entry_t* prdt = allocate_prdt(1);
    if (!prdt) {
        // free_command_header(cmd_header);
        return -1;
    }

    // 1. Fill command FIS (Corrected for 48-bit LBA if needed)
    cmd_header->cfis[0] = 0x20; // ATA command: Read (0x20 for 28-bit, 0x24 for 48-bit)

    if (lba > 0xFFFFFFF || (lba + sector_count) > 0xFFFFFFF) { // Check for 28-bit overflow
      cmd_header->cfis[0] = 0x24; // Use 48-bit command if needed
      cmd_header->cfis[1] = 0;
      cmd_header->cfis[2] = lba & 0xFF;
      cmd_header->cfis[3] = (lba >> 8) & 0xFF;
      cmd_header->cfis[4] = (lba >> 16) & 0xFF;
      cmd_header->cfis[5] = (lba >> 24) & 0xFF;
      cmd_header->cfis[6] = (lba >> 32) & 0xFF; // LBA 48 bit
      cmd_header->cfis[7] = (lba >> 40) & 0xFF;  // LBA 48 bit
      cmd_header->cfis[8] = sector_count & 0xFF;
      cmd_header->cfis[9] = (sector_count >> 8) & 0xFF; // Sector count 16 bit
    } else {
      cmd_header->cfis[2] = lba & 0xFF;
      cmd_header->cfis[3] = (lba >> 8) & 0xFF;
      cmd_header->cfis[4] = (lba >> 16) & 0xFF;
      cmd_header->cfis[5] = (lba >> 24) & 0xFF;
      cmd_header->cfis[7] = sector_count & 0xFF;
      cmd_header->cfis[8] = (sector_count >> 8) & 0xFF;
    }


    // 2. Fill PRDT (Corrected DBC)
    prdt[0].dba = (uint32_t)buffer;
    prdt[0].dbc = sector_count * SECTOR_SIZE - 1; // Byte count, not sector count

    // 3. Set up command header
    cmd_header->prdtl = 1; // Number of PRDT entries, not size in bytes.
    cmd_header->prdt = (uint32_t)prdt;

    // 4. Issue the command
    cmd_header->ci = 1;

    // 5. **CRITICAL:** Wait for command completion.  This is the most likely source of your original problem.
    wait_for_command_completion(cmd_header);

    // 6. Check for errors (add this!)
    if (cmd_header->ciss & 0x1) { // Error bit set
    //   free_command_header(cmd_header);
      free_prdt(prdt);
        return -1; // Or a more specific error code.
    }

    // free_command_header(cmd_header);
    free_prdt(prdt);

    return 0;
}

void wait_for_command_completion(ahci_command_header_t* cmd_header) {
    volatile uint32_t* status_reg = (volatile uint32_t*)(cmd_header + 0x08); // Example, adjust as needed.
      while (1) {
          if (! (status_reg[0] & (1 << 7))) { // Check BSY (Busy) bit in PxSATASTATUS register
            break;
          }
      }
  
      // Check the command complete (CC) bit in the command header.
    //   while (cmd_header->ci) ; // Wait for CI to clear (command complete)
  }