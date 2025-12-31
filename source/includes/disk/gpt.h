#ifndef __GPT_H
#define __GPT_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <disk/mbr.h>

#define GPT_EFI_PMBR_SECTOR             0
#define GPT_EFI_PART_HEADER_SECTOR      1
#define GPT_PARTITION_ENTRIES_SECTOR    2
#define GPT_PART_ATTRIB_FIRMWARE        0b1
#define GPT_PART_ATTRIB_USED_BY_OS      0b10
#define GPT_ENTRIES_SIZE                (512*31)/sizeof(GPT_PartitionEntry)

extern uint8_t GPT_EFI_SIGNATURE[]; // it's just "EFI PART"

struct GPT_PartTableHeader{
    uint8_t                  Signature[8];   // MUST BE "EFI PART"
    uint32_t                 Revision;
    uint32_t                 HeaderSize;
    uint32_t                 CRC32_Checksum;
    uint32_t                 Reserved0;
    uint64_t                 ThisHdrLBA;
    uint64_t                 AlternateHdrLBA;
    uint64_t                 FirstUsableGptEntryBlock;
    uint64_t                 LastUsableGptEntryBlock;
    uint8_t                  GUID[16];
    uint64_t                 StartLBA_GUID_Entries;
    uint32_t                 NumEntries;
    uint32_t                 PartitionEntrySize; // In bytes!
    uint32_t                 CRC32_PartitionEntries;
    uint8_t                  Reserved1[512-0x5C];    // Should be zeroed
} __attribute__((packed)) ;

struct GPT_PartitionEntry {
    uint8_t   PartitionTypeGUID[16];   // 0 = unused
    uint8_t   UniqueGUID[16];
    uint64_t  StartLBA;
    uint64_t  EndLBA;
    uint64_t  Attributes;
    uint16_t  PartitionName[36];       // UTF-16LE
} __attribute__((packed));

typedef struct {
    int port;
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t sectors;
    uint8_t type_guid[16];
} gpt_partition_t;

typedef struct {
    int port;
    uint64_t sectors;
    gpt_partition_t partitions[128]; // max typical
    int partition_count;
} gpt_disk_t;

extern gpt_disk_t gpt_disks[10];
extern int gpt_disks_count;

int check_gpt(int portno);
void parse_gpt_partitions(int portno, struct GPT_PartTableHeader* hdr);
bool gpt_is_uefi_bootable(const struct GPT_PartitionEntry* p);

#endif