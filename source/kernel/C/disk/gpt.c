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

static const uint8_t GPT_ESP_GUID[16] = {
    0x28,0x73,0x2A,0xC1,
    0x1F,0xF8,
    0xD2,0x11,
    0xBA,0x4B,
    0x00,0xA0,0xC9,0x3E,0xC9,0x3B
};

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

        bool bootable = gpt_is_uefi_bootable(p);

        partition_fs_type_t fs_type = detect_fat_type_enum(bs);

        char part_name[64];
        snprintf(part_name, sizeof(part_name), "disk%up%u", portno, i+1);

        add_general_partition(
            PART_TABLE_GPT,
            start,
            end,
            end - start + 1,
            portno,
            bootable,
            fs_type,
            part_name,
            null,
            p->PartitionTypeGUID
        );

        printf("[AHCI/GPT] identified disk %s", part_name);
        if (fs_type == FS_FAT16) {
            // fat16_fs_t fs;
            // fat16_dir_entry_t file;

            // //
            // fat16_mount(portno, start, &fs);
            
            // fat16_create(&fs, 0, "FWLOGS.TXT", 0x20);
            // fat16_file_t f;
            // fat16_open(&fs, "/FWLOGS.TXT", &f);
            // const char msg[] = "HELLO FAT16\n";
            // fat16_write(&f, (const uint8_t*)msg, sizeof(msg));

            // // rewind
            // f.pos = 0;
            // f.cluster = f.entry.first_cluster;

            // uint8_t buf[64];
            // fat16_read(&f, buf, sizeof(buf));
            // for(int k=0;k<64;k++)
            //     printfnoln("%c", buf[k]);

            // fat16_create_path(&fs, "/a/b/c/text.txt", 0x20); // 0x20 = archive
            
            // fat16_file_t fnew;
            // if (fat16_open(&fs, "/a/b/c/text.txt", &fnew) == 0) {
            //     const char msg1[] = "HELLO FAT16 PATH!\n";
            //     fat16_write(&fnew, (const uint8_t*)msg1, sizeof(msg1)-1); // -1 to skip null terminator
            //     fat16_close(&fnew);
            // }

            // print("\n");
        }
    }

    gpt_disks_count++;
}

bool gpt_is_uefi_bootable(const struct GPT_PartitionEntry* p) {
    return memcmp(p->PartitionTypeGUID, GPT_ESP_GUID, 16) == 0;
}
