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
            if (sig == 0x00000101) {
                // SATA device detected
                info("SATA Disk detected!", __FILE__);
            } else if (sig == 0xEB140101) {
                // SATAPI device detected
                info("SATAPI Disk detected!", __FILE__);
            } else if (sig == 0xC33C0101) {
                // SEMB device detected
                info("SEMB Disk detected!", __FILE__);
            } else if (sig == 0x96690101) {
                // Port Multiplier (PM) device detected
                info("Port Multiplier (PM) Disk detected!", __FILE__);
            }else{
                warn("Unknown disk detected!", __FILE__);
                printf("port->sig = 0x%x", sig);
            }
        }
    }
}
