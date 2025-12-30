/**
 * @file gpt.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The main code for GPT AHCI Disks.
 * @version 0.1
 * @date 2025-12-29
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <disk/gpt.h>
#include <ahci.h>
#include <filesystems/fat16.h>

int gpt_disks_count = 0;
gpt_disk_t gpt_disks[10];

int check_gpt(int portno) {
    uint8_t* buf = kmalloc_aligned(512, 512);
    if (!buf) {
        error("[AHCI/GPT] kmalloc failed", __FILE__);
        return -1;
    }

    ahci_read_sector(portno, 0, buf, 1);

    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        info("[GPT] Invalid PMBR signature", __FILE__);
        return -2;
    }

    mbr_partition_t* pmbr = (mbr_partition_t*)&buf[446];
    if (pmbr[0].partition_type != 0xEE) {
        info("[GPT] Not a GPT disk (no 0xEE PMBR)", __FILE__);
        return -3;
    }

    ahci_read_sector(portno, 1, buf, 1);
    struct GPT_PartTableHeader* hdr = (struct GPT_PartTableHeader*)buf;

    if (memcmp(hdr->Signature, "EFI PART", 8) != 0) {
        error("[GPT] Invalid GPT signature", __FILE__);
        return -4;
    }

    info("[GPT] Valid GPT detected", __FILE__);
    parse_gpt_partitions(portno, hdr);
    return 0;
}

void parse_gpt_partitions(int portno, struct GPT_PartTableHeader* hdr) {
    uint32_t entry_size = hdr->PartitionEntrySize;
    uint32_t count = hdr->NumEntries;
    uint64_t lba = hdr->StartLBA_GUID_Entries;

    uint32_t total_bytes = entry_size * count;
    uint32_t sectors = (total_bytes + 511) / 512;

    uint8_t* buf = kmalloc_aligned(sectors * 512, 512);
    ahci_read_sector(portno, lba, buf, sectors);

    gpt_disk_t* disk = &gpt_disks[gpt_disks_count];
    disk->port = portno;
    disk->sectors = ahci_disks[portno].total_sectors;
    disk->partition_count = 0;

    for (uint32_t i = 0; i < count; i++) {
        struct GPT_PartitionEntry* p =
            (struct GPT_PartitionEntry*)(buf + i * entry_size);

        /* Unused entry */
        if (memcmp(p->PartitionTypeGUID, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0)
            continue;

        uint64_t start = p->StartLBA;
        uint64_t end = p->EndLBA;

        printf("GPT Partition %d: start=%u end=%u size=%d",
            disk->partition_count,
            start,
            end,
            end - start + 1
        );

        gpt_partition_t* out = &disk->partitions[disk->partition_count++];
        out->port = portno;
        out->start_lba = start;
        out->end_lba = end;
        out->sectors = end - start + 1;
        memcpy(out->type_guid, p->PartitionTypeGUID, 16);

        uint8_t bs[512];
        ahci_read_sector(portno, start, bs, 1);

        int fat_type = detect_fat_type(bs);
        printf("Detected FAT type: %d", fat_type);
        if (fat_type == 16) {
            fat16_fs_t fs;
            fat16_dir_entry_t file;

            //
            fat16_mount(portno, start, &fs);
            
            fat16_create(&fs, 0, "FWLOGS.TXT", 0x20);
            fat16_file_t f;
            fat16_open(&fs, "/FWLOGS.TXT", &f);
            const char msg[] = "HELLO FAT16\n";
            fat16_write(&f, (const uint8_t*)msg, sizeof(msg));

            // rewind
            f.pos = 0;
            f.cluster = f.entry.first_cluster;
            uint8_t buf[64];
            fat16_read(&f, buf, sizeof(buf));
            for(int k=0;k<64;k++)
                printfnoln("%c", buf[k]);
            print("\n");
        }
    }

    gpt_disks_count++;
}
