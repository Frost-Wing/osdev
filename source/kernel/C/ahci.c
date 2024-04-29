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

void detect_ahci_devices(ahci_controller* ahci_ctrl) {
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
