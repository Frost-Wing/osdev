#include <drivers/sata.h>

int16 ahci_bar = 0;

#define HBA_GHC         0x04      // Global Host Control
#define HBA_PxSIG       0x24      // Port Signature
#define HBA_PxCMD       0x0       // Port Command List Base Address


// AHCI Controller Memory-Mapped Registers
volatile uint32_t* hba_mem;

// AHCI Device Types
#define AHCI_DEV_SATA   0
#define AHCI_DEV_SATAPI 1
#define AHCI_DEV_SEMB   2
#define AHCI_DEV_PM     3

/**
 * @brief Read the SATA drive signature on a specific port
 * @param port_num Port number
 * @return SATA drive signature
 */
uint32_t read_sata_signature(uint8_t port_num) {
    return hba_mem[(port_num * 0x80) / sizeof(uint32_t) + (HBA_PxSIG / sizeof(uint32_t))];
}

/**
 * @brief Check the type of device connected to a specific port
 * @param port Pointer to the AHCI port structure
 * @return Device type (AHCI_DEV_SATA, AHCI_DEV_SATAPI, AHCI_DEV_SEMB, AHCI_DEV_PM)
 */
int check_type(volatile uint32_t* port) {
    uint32_t ssts = port[HBA_PxSIG];
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;

    if (det != 3) {
        // No device present
        return -1;
    }

    if (ipm == 1 || ipm == 2) {
        // SATA device
        return AHCI_DEV_SATA;
    } else if (ipm == 3) {
        // SATAPI device
        return AHCI_DEV_SATAPI;
    } else if (ipm == 4) {
        // SEMB device
        return AHCI_DEV_SEMB;
    } else if (ipm == 5) {
        // PM device
        return AHCI_DEV_PM;
    } else {
        // Unknown device type
        return -1;
    }
}

/**
 * @brief AHCI Initialization
 * 
 */
void ahci_init() {
    if(ahci_bar == 0 || ahci_bar == null){
        warn("No AHCI controllers found.", __FILE__);
        return;
    }
    hba_mem = (volatile uint32_t*)ahci_bar;

    hba_mem[HBA_GHC] |= (1 << 31);

    for (int i = 0; i < 32; ++i) {
        int dt = check_type(&hba_mem[HBA_PxCMD + i * 32]);
        if (dt == AHCI_DEV_SATA) {
            printf("SATA drive found at port %d\n", i);
        } else if (dt == AHCI_DEV_SATAPI) {
            printf("SATAPI drive found at port %d\n", i);
        } else if (dt == AHCI_DEV_SEMB) {
            printf("SEMB drive found at port %d\n", i);
        } else if (dt == AHCI_DEV_PM) {
            printf("PM drive found at port %d\n", i);
        } else {
            printf("No drive found at port %d\n", i);
        }
    }
}