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
ahci_disk_info_t ahci_disks[32];
general_partition_t ahci_partitions[MAX_PARTITIONS];
int general_partition_count = 0;

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

        printf("port->sig = 0x%x", port->sig);

        switch (sig) {
            case sata_disk:
                printf("[AHCI] SATA Disk detected on port %d", i);
                
                handle_sata_disk(i);
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

void handle_sata_disk(int portno) {
    ahci_init_port(portno);

    int16* id = kmalloc_aligned(512, 4096);

    if (!id) {
        error("[AHCI] Allocation failed!", __FILE__);
        return;
    }

    if (ahci_identify(portno, id) != 0) {
        printf("[AHCI] IDENTIFY failed on port %d", portno);
        return;
    }

    uint64_t sectors = ((uint64_t)id[103] << 48) | ((uint64_t)id[102] << 32) | ((uint64_t)id[101] << 16) | ((uint64_t)id[100]);

    ahci_disks[portno].total_sectors = sectors;
    ahci_disks[portno].present = 1;

    if(check_gpt(portno) == 0) return;
    check_mbr(portno);

    // char* msg = "FROSTWING WAS HERE";
    // memset(buf, 0, 512);
    // memcpy(buf, msg, strlen(msg));

    // uint64_t write_lba = 2; // or 10 for testing

    // if (ahci_write_sector(portno, 0, buf, 1) != 0) {
    //     printf("[AHCI] Write LBA failed");
    //     return;
    // }

    // memset(buf, 0, 512);
    // if (ahci_read_sector(portno, 0, buf, 1) != 0) {
    //     printf("[AHCI] Read LBA failed");
    //     return;
    // }

    // for (int i = 0; i < 32; i++) printfnoln("%c", buf[i]);

    // print("\n");
}

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
) {
    if (general_partition_count >= MAX_PARTITIONS)
        return NULL;

    general_partition_t* p = &ahci_partitions[general_partition_count++];
    memset(p, 0, sizeof(general_partition_t));

    p->table_type   = table_type;
    p->lba_start    = lba_start;
    p->lba_end      = lba_end;
    p->sector_count = sector_count;
    p->ahci_port    = ahci_port;
    p->bootable     = bootable;
    p->fs_type      = fs_type;

    switch (table_type) {
        case PART_TABLE_MBR:
            p->mbr_type = mbr_type;
            break;

        case PART_TABLE_GPT:
            if (gpt_guid)
                memcpy(p->gpt_type_guid, gpt_guid, 16);
            break;
    }

    if (name) {
        strncpy(p->name, name, sizeof(p->name) - 1);
        p->name[sizeof(p->name) - 1] = '\0';
    } else {
        strcpy(p->name, "unknown");
    }

    return p;
}

general_partition_t* search_general_partition(cstring partition_name) {
    if (!partition_name)
        return NULL;

    for (size_t i = 0; i < general_partition_count; i++) {
        if (strcmp(ahci_partitions[i].name, partition_name) == 0) {
            return &ahci_partitions[i];
        }
    }

    return NULL;  // not found
}

void ahci_init_port(int portno) {
    ahci_port_t* port = &global_ahci_ctrl->ports[portno];
    ahci_port_mem_t* mem = &port_mem[portno];

    port->cmd &= ~AHCI_PORT_CMD_ST;
    while (port->cmd & AHCI_PORT_CMD_CR);

    port->cmd &= ~AHCI_PORT_CMD_FRE;
    while (port->cmd & AHCI_PORT_CMD_FR);

    mem->cmd_list = kmalloc_aligned(1024, 1024);
    memset(mem->cmd_list, 0, 1024);
    port->clb  = (uint32_t)(uintptr_t)mem->cmd_list;
    port->clbu = 0;

    mem->fis = kmalloc_aligned(256, 256);
    memset(mem->fis, 0, 256);
    port->fb = (uint32_t)(uintptr_t)mem->fis;
    port->fbu = 0;

    for (int i = 0; i < 32; i++) {
        mem->cmd_tables[i] = kmalloc_aligned(sizeof(ahci_cmd_table_t), 128);
        memset(mem->cmd_tables[i], 0, sizeof(ahci_cmd_table_t));
        mem->cmd_list[i].ctba = (uint32_t)(uintptr_t)mem->cmd_tables[i];
        mem->cmd_list[i].ctbau = 0;
    }

    port->serr = 0xFFFFFFFF;
    port->is   = 0xFFFFFFFF;

    port->cmd |= AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST;
}

static int ahci_find_slot(ahci_port_t* port) {
    uint32_t slots = port->ci | port->sact;
    for (int i = 0; i < 32; i++) {
        if (!(slots & (1 << i))) return i;
    }
    return -1;
}

int ahci_read_sector(int portno, uint64_t lba, void* buffer, uint32_t count) {
    ahci_port_t* port = &global_ahci_ctrl->ports[portno];
    ahci_port_mem_t* mem = &port_mem[portno];

    while (port->tfd & (0x80 | 0x08));

    int slot = ahci_find_slot(port);
    if (slot == -1) return -1;

    ahci_cmd_header_t* hdr = &mem->cmd_list[slot];
    memset(hdr, 0, sizeof(*hdr));
    hdr->flags = 5;
    hdr->prdtl = 1;
    hdr->ctba = (uint32_t)(uintptr_t)mem->cmd_tables[slot];
    hdr->ctbau = 0;

    ahci_cmd_table_t* tbl = mem->cmd_tables[slot];
    memset(tbl->cfis, 0, 64);
    memset(tbl->prdt, 0, sizeof(tbl->prdt));

    tbl->prdt[0].dba  = (uint32_t)(uintptr_t)buffer;
    tbl->prdt[0].dbau = 0;
    tbl->prdt[0].dbc  = (count * 512 - 1) | (1 << 31);

    uint8_t* cfis = tbl->cfis;
    cfis[0] = 0x27;
    cfis[1] = 1 << 7;
    cfis[2] = READ_DMA_EXT;

    cfis[4] = (uint8_t)lba;
    cfis[5] = (uint8_t)(lba >> 8);
    cfis[6] = (uint8_t)(lba >> 16);
    cfis[7] = 0x40;
    cfis[8] = (uint8_t)(lba >> 24);
    cfis[9] = (uint8_t)(lba >> 32);
    cfis[10] = (uint8_t)(lba >> 40);
    cfis[12] = count & 0xFF;
    cfis[13] = (count >> 8) & 0xFF;

    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    port->ci = 1 << slot;

    while (port->ci & (1 << slot)) {
        if (port->tfd & (0x01 | 0x20)) return -2;
    }

    port->is = 0xFFFFFFFF;
    return 0;
}

int ahci_write_sector(int portno, uint64_t lba, void* buffer, uint32_t count) {
    ahci_port_t* port = &global_ahci_ctrl->ports[portno];
    ahci_port_mem_t* mem = &port_mem[portno];

    while (port->tfd & (0x80 | 0x08));

    int slot = ahci_find_slot(port);
    if (slot == -1) return -1;

    ahci_cmd_header_t* hdr = &mem->cmd_list[slot];
    memset(hdr, 0, sizeof(*hdr));
    hdr->flags = 5 | AHCI_CMD_HDR_W_BIT;
    hdr->prdtl = 1;
    hdr->ctba = (uint32_t)(uintptr_t)mem->cmd_tables[slot];
    hdr->ctbau = 0;

    ahci_cmd_table_t* tbl = mem->cmd_tables[slot];
    memset(tbl->cfis, 0, 64);
    memset(tbl->prdt, 0, sizeof(tbl->prdt));

    tbl->prdt[0].dba  = (uint32_t)(uintptr_t)buffer;
    tbl->prdt[0].dbau = 0;
    tbl->prdt[0].dbc  = (count * 512 - 1) | (1 << 31);

    uint8_t* cfis = tbl->cfis;
    cfis[0] = 0x27;
    cfis[1] = 1 << 7;
    cfis[2] = ATA_CMD_WRITE_DMA_EXT;

    cfis[4] = (uint8_t)lba;
    cfis[5] = (uint8_t)(lba >> 8);
    cfis[6] = (uint8_t)(lba >> 16);
    cfis[7] = 0x40;
    cfis[8] = (uint8_t)(lba >> 24);
    cfis[9] = (uint8_t)(lba >> 32);
    cfis[10] = (uint8_t)(lba >> 40);
    cfis[12] = count & 0xFF;
    cfis[13] = (count >> 8) & 0xFF;

    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    port->ci = 1 << slot;

    while (port->ci & (1 << slot)) {
        if (port->tfd & (0x01 | 0x20)) { // ERR | DF
            port->is = 0xFFFFFFFF;
            printf("[AHCI] Write error! tfd=0x%X\n", port->tfd);
            return -3;
        }
    }

    port->is = 0xFFFFFFFF;
    return 0;
}

int ahci_identify(int portno, void* buffer) {
    ahci_port_t* port = &global_ahci_ctrl->ports[portno];
    ahci_port_mem_t* mem = &port_mem[portno];

    // Wait until port is ready
    while (port->tfd & (0x80 | 0x08)); // BSY | DRQ

    int slot = ahci_find_slot(port);
    if (slot == -1) return -1;

    ahci_cmd_header_t* hdr = &mem->cmd_list[slot];
    memset(hdr, 0, sizeof(*hdr));
    hdr->flags = 5; 
    hdr->prdtl = 1; // one PRDT entry
    hdr->ctba = (uint32_t)(uintptr_t)mem->cmd_tables[slot];
    hdr->ctbau = 0;


    ahci_cmd_table_t* tbl = mem->cmd_tables[slot];
    memset(tbl->cfis, 0, 64);
    memset(tbl->prdt, 0, sizeof(tbl->prdt));


    tbl->prdt[0].dba  = (uint32_t)(uintptr_t)buffer;
    tbl->prdt[0].dbau = 0;
    tbl->prdt[0].dbc  = (512 - 1) | (1 << 31); // 512 bytes, IOC

    uint8_t* cfis = tbl->cfis;
    cfis[0] = 0x27;        // Host to device
    cfis[1] = 1 << 7;      // Command
    cfis[2] = ATA_CMD_IDENTIFY; // IDENTIFY DEVICE
    cfis[7] = 0;           // Features = 0
    cfis[8] = 0; cfis[9] = 0; cfis[10] = 0; // LBA = 0

    port->serr = 0xFFFFFFFF;
    port->is   = 0xFFFFFFFF;

    port->ci = 1 << slot;

    // Wait for completion
    while (port->ci & (1 << slot)) {
        if (port->tfd & (0x01 | 0x20)) { // ERR | DF
            port->is = 0xFFFFFFFF;
            return -2;
        }
    }

    port->is = 0xFFFFFFFF;
    return 0;
}
