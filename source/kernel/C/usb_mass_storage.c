#include <ahci.h>
#include <disk/gpt.h>
#include <filesystems/ext2.h>
#include <filesystems/fat16.h>
#include <filesystems/iso9660.h>
#include <filesystems/vfs.h>
#include <graphics.h>
#include <heap.h>
#include <memory.h>
#include <strings.h>
#include <usb_mass_storage.h>
#include <xhci.h>

#define USB_MSC_CLASS 0x08U
#define USB_MSC_SCSI_TRANSPARENT 0x06U
#define USB_MSC_BULK_ONLY 0x50U
#define USB_MSC_GET_MAX_LUN 0xFEU
#define USB_MSC_RESET 0xFFU
#define USB_MSC_CBW_SIGNATURE 0x43425355U
#define USB_MSC_CSW_SIGNATURE 0x53425355U
#define USB_MSC_MAX_LUNS 16U
#define USB_MSC_MAX_TARGETS 64U
#define USB_MSC_MAX_GPT_ENTRIES 128U
#define USB_MSC_MAX_TRANSFER 0x1FFFFU

typedef struct __attribute__((packed)) {
    uint32_t signature;
    uint32_t tag;
    uint32_t transfer_length;
    uint8_t flags;
    uint8_t lun;
    uint8_t command_length;
    uint8_t command[16];
} usb_msc_cbw_t;

typedef struct __attribute__((packed)) {
    uint32_t signature;
    uint32_t tag;
    uint32_t residue;
    uint8_t status;
} usb_msc_csw_t;

typedef struct {
    usb_device_t *device;
    uint8_t interface_number;
    uint8_t in_address;
    uint8_t out_address;
    uint8_t in_endpoint;
    uint8_t out_endpoint;
    uint8_t max_lun;
    uint32_t next_tag;
    volatile int lock;
    uint8_t active;
} usb_msc_transport_t;

typedef struct {
    usb_msc_transport_t *transport;
    uint8_t lun;
    uint8_t active;
    int block_id;
    uint32_t block_size;
    uint64_t block_count;
    uint8_t media_present;
    uint8_t partitions_scanned;
} usb_msc_target_t;

static usb_msc_transport_t transports[USB_MSC_MAX_TARGETS];
static usb_msc_target_t targets[USB_MSC_MAX_TARGETS];
static uint32_t next_usb_disk_number;

static void msc_lock(usb_msc_transport_t *transport) {
    while (__sync_lock_test_and_set(&transport->lock, 1))
        __asm__ volatile("pause");
}

static void msc_unlock(usb_msc_transport_t *transport) {
    __sync_lock_release(&transport->lock);
}

static int msc_bulk(usb_msc_transport_t *transport, uint8_t endpoint,
    void *buffer, uint32_t length, uint32_t *actual) {
    uint32_t done = 0;
    while (done < length) {
        uint32_t chunk = length - done;
        if (chunk > USB_MSC_MAX_TRANSFER)
            chunk = USB_MSC_MAX_TRANSFER;
        uint32_t transferred = 0;
        if (xhci_bulk_transfer(transport->device->slot_id, endpoint,
                (uint8_t *)buffer + done, chunk, &transferred) != 0)
            return -1;
        if (transferred > chunk)
            return -1;
        done += transferred;
        if (transferred != chunk)
            break;
    }
    if (actual)
        *actual = done;
    return 0;
}

static int msc_reset_recovery(usb_msc_transport_t *transport) {
    int result = usb_control_request(transport->device, 0x21U,
        USB_MSC_RESET, 0, transport->interface_number, NULL, 0);
    int in_result = usb_control_request(transport->device, 0x02U, 1U, 0,
        transport->in_address, NULL, 0);
    int out_result = usb_control_request(transport->device, 0x02U, 1U, 0,
        transport->out_address, NULL, 0);
    int in_reset = xhci_reset_endpoint(transport->device->slot_id,
        transport->in_endpoint);
    int out_reset = xhci_reset_endpoint(transport->device->slot_id,
        transport->out_endpoint);
    return result == 0 && in_result == 0 && out_result == 0 &&
        in_reset == 0 && out_reset == 0 ? 0 : -1;
}

/* Returns 0 for passed CSW, 1 for failed command, and -1 for transport error. */
static int msc_command(usb_msc_target_t *target, const uint8_t *cdb,
    uint8_t cdb_length, uint8_t direction, void *data, uint32_t data_length,
    uint32_t *transferred_out) {
    usb_msc_transport_t *transport = target->transport;
    if (!transport || !transport->active || !target->active ||
        cdb_length == 0 || cdb_length > 16 ||
        (data_length && !data) || (direction != 0 && direction != 0x80U))
        return -1;

    usb_msc_cbw_t cbw;
    memset(&cbw, 0, sizeof(cbw));
    cbw.signature = USB_MSC_CBW_SIGNATURE;
    cbw.tag = ++transport->next_tag;
    if (cbw.tag == 0)
        cbw.tag = ++transport->next_tag;
    cbw.transfer_length = data_length;
    cbw.flags = direction;
    cbw.lun = target->lun;
    cbw.command_length = cdb_length;
    memcpy(cbw.command, cdb, cdb_length);

    uint32_t actual = 0;
    uint8_t *cbw_buffer = kmalloc_aligned(sizeof(cbw), 16);
    uint8_t *csw_buffer = kmalloc_aligned(sizeof(usb_msc_csw_t), 16);
    if (!cbw_buffer || !csw_buffer) {
        if (cbw_buffer)
            kfree(cbw_buffer);
        if (csw_buffer)
            kfree(csw_buffer);
        return -1;
    }
    memcpy(cbw_buffer, &cbw, sizeof(cbw));
    int result = msc_bulk(transport, transport->out_endpoint, cbw_buffer,
        sizeof(cbw), &actual);
    if (result != 0 || actual != sizeof(cbw))
        goto transport_error;

    actual = 0;
    if (data_length) {
        result = msc_bulk(transport,
            direction == 0x80U ? transport->in_endpoint : transport->out_endpoint,
            data, data_length, &actual);
        if (result != 0)
            goto transport_error;
    }

    memset(csw_buffer, 0, sizeof(usb_msc_csw_t));
    uint32_t csw_actual = 0;
    result = msc_bulk(transport, transport->in_endpoint, csw_buffer,
        sizeof(usb_msc_csw_t), &csw_actual);
    if (result != 0 || csw_actual != sizeof(usb_msc_csw_t))
        goto transport_error;

    usb_msc_csw_t csw;
    memcpy(&csw, csw_buffer, sizeof(csw));
    if (csw.signature != USB_MSC_CSW_SIGNATURE || csw.tag != cbw.tag ||
        csw.residue > data_length || actual > data_length ||
        actual + csw.residue != data_length || csw.status > 2U)
        goto transport_error;
    if (transferred_out)
        *transferred_out = actual;
    if (csw.status == 2U)
        goto transport_error;

    kfree(cbw_buffer);
    kfree(csw_buffer);
    return csw.status == 0 ? 0 : 1;

transport_error:
    if (msc_reset_recovery(transport) != 0)
        error("USB BOT reset recovery failed for interface %u", __FILE__,
            transport->interface_number);
    kfree(cbw_buffer);
    kfree(csw_buffer);
    return -1;
}

static int msc_request_sense(usb_msc_target_t *target, uint8_t *sense_key,
    uint8_t *asc, uint8_t *ascq) {
    uint8_t cdb[6] = {0x03U, (uint8_t)(target->lun << 5), 0, 0, 18, 0};
    uint8_t sense[18] __attribute__((aligned(16)));
    memset(sense, 0, sizeof(sense));
    uint32_t actual = 0;
    if (msc_command(target, cdb, sizeof(cdb), 0x80U, sense,
            sizeof(sense), &actual) != 0 || actual < 14U)
        return -1;
    if (sense_key)
        *sense_key = sense[2] & 0x0FU;
    if (asc)
        *asc = sense[12];
    if (ascq)
        *ascq = sense[13];
    return 0;
}

static int msc_inquiry(usb_msc_target_t *target) {
    uint8_t cdb[6] = {0x12U, 0, 0, 0, 36, 0};
    uint8_t inquiry[36] __attribute__((aligned(16)));
    memset(inquiry, 0, sizeof(inquiry));
    uint32_t actual = 0;
    if (msc_command(target, cdb, sizeof(cdb), 0x80U, inquiry,
            sizeof(inquiry), &actual) != 0 || actual < sizeof(inquiry))
        return -1;
    info("USB SCSI LUN %u: peripheral type 0x%02x, %.8s %.16s",
        __FILE__, target->lun, inquiry[0] & 0x1FU, &inquiry[8], &inquiry[16]);
    return 0;
}

static int msc_test_unit_ready(usb_msc_target_t *target) {
    uint8_t cdb[6] = {0};
    return msc_command(target, cdb, sizeof(cdb), 0, NULL, 0, NULL);
}

static int msc_wait_ready(usb_msc_target_t *target) {
    for (unsigned int attempt = 0; attempt < 3; ++attempt) {
        int result = msc_test_unit_ready(target);
        if (result == 0)
            return 0;
        if (result < 0)
            return -1;
        uint8_t key = 0, asc = 0, ascq = 0;
        if (msc_request_sense(target, &key, &asc, &ascq) != 0)
            return -1;
        if (key == 0x06U || (key == 0x02U && asc == 0x04U))
            continue;
        if (key == 0x02U && asc == 0x3AU)
            return -1;
    }
    return -1;
}

static int msc_read_capacity(usb_msc_target_t *target) {
    uint8_t cdb[10] = {0x25U};
    uint8_t response[8] __attribute__((aligned(16)));
    memset(response, 0, sizeof(response));
    uint32_t actual = 0;
    int result = msc_command(target, cdb, sizeof(cdb), 0x80U, response,
        sizeof(response), &actual);
    if (result != 0 || actual != sizeof(response))
        return -1;

    uint32_t last_lba = ((uint32_t)response[0] << 24) |
        ((uint32_t)response[1] << 16) |
        ((uint32_t)response[2] << 8) | response[3];
    uint32_t block_size = ((uint32_t)response[4] << 24) |
        ((uint32_t)response[5] << 16) |
        ((uint32_t)response[6] << 8) | response[7];
    if (block_size < 512U || block_size > 65536U)
        return -1;
    target->block_size = block_size;
    target->block_count = (uint64_t)last_lba + 1U;
    target->media_present = 1;
    block_device_info_t *dev = block_get_device(target->block_id);
    if (dev) {
        dev->sector_size = block_size;
        dev->total_sectors = target->block_count;
    }
    return 0;
}

static partition_fs_type_t msc_detect_fs(int device_id, uint64_t start) {
    if (get_block_size(device_id) != 512U)
        return FS_UNKNOWN;
    uint8_t *sector = kmalloc_aligned(get_block_size(device_id), 16);
    if (!sector)
        return FS_UNKNOWN;
    partition_fs_type_t type = FS_UNKNOWN;
    if (block_read_sector(device_id, start, sector, 1) == 0)
        type = detect_fat_type_enum(sector);
    kfree(sector);
    if (type == FS_UNKNOWN && start <= UINT32_MAX)
        type = detect_ext2_type_enum(device_id, (uint32_t)start);
    if (type == FS_UNKNOWN && start <= UINT32_MAX &&
        iso9660_detect_at_lba(device_id, (uint32_t)start))
        type = FS_ISO9660;
    return type;
}

static void msc_add_partition(usb_msc_target_t *target, uint64_t start,
    uint64_t count, partition_table_type_t table_type, bool bootable,
    uint8_t mbr_type, const uint8_t *guid, unsigned int ordinal) {
    if (!count || start >= target->block_count ||
        count > target->block_count - start)
        return;
    char name[64];
    block_device_info_t *dev = block_get_device(target->block_id);
    if (!dev)
        return;
    snprintf(name, sizeof(name), "%sp%u", dev->name, ordinal);
    (void)add_general_partition(table_type, start, start + count - 1U,
        count, (uint64_t)target->block_id, bootable,
        msc_detect_fs(target->block_id, start), name, mbr_type, guid);
}

static uint32_t msc_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int32_t)(crc & 1U));
    }
    return ~crc;
}

static int msc_parse_gpt(usb_msc_target_t *target, const uint8_t *mbr) {
    if (mbr[510] != 0x55U || mbr[511] != 0xAAU)
        return -1;
    const mbr_partition_t *entries = (const mbr_partition_t *)(mbr + 446);
    bool protective = false;
    for (unsigned int i = 0; i < 4; ++i)
        protective |= entries[i].partition_type == 0xEEU;
    if (!protective || target->block_count < 2)
        return -1;

    uint8_t *header_sector = kmalloc_aligned(target->block_size, 16);
    if (!header_sector)
        return -1;
    int result = -1;
    if (block_read_sector(target->block_id, 1, header_sector, 1) != 0)
        goto done;

    struct GPT_PartTableHeader header;
    memcpy(&header, header_sector, sizeof(header));
    if (memcmp(header.Signature, "EFI PART", 8) != 0 ||
        header.HeaderSize < 92U || header.HeaderSize > sizeof(header) ||
        header.HeaderSize > target->block_size ||
        header.NumEntries == 0 || header.NumEntries > USB_MSC_MAX_GPT_ENTRIES ||
        header.PartitionEntrySize < sizeof(struct GPT_PartitionEntry) ||
        header.PartitionEntrySize > 4096U ||
        header.StartLBA_GUID_Entries >= target->block_count ||
        header.NumEntries > SIZE_MAX / header.PartitionEntrySize)
        goto done;

    uint32_t saved_crc = header.CRC32_Checksum;
    header.CRC32_Checksum = 0;
    if (msc_crc32((const uint8_t *)&header, header.HeaderSize) != saved_crc)
        goto done;

    size_t entries_bytes = (size_t)header.NumEntries *
        header.PartitionEntrySize;
    size_t sector_count = (entries_bytes + target->block_size - 1U) /
        target->block_size;
    if (!sector_count || sector_count > SIZE_MAX / target->block_size ||
        sector_count > target->block_count - header.StartLBA_GUID_Entries)
        goto done;
    uint8_t *partition_data = kmalloc_aligned(
        sector_count * target->block_size, 16);
    if (!partition_data)
        goto done;
    if (block_read_sector(target->block_id, header.StartLBA_GUID_Entries,
            partition_data, (uint32_t)sector_count) != 0 ||
        msc_crc32(partition_data, entries_bytes) !=
            header.CRC32_PartitionEntries) {
        kfree(partition_data);
        goto done;
    }

    unsigned int ordinal = 0;
    for (uint32_t i = 0; i < header.NumEntries; ++i) {
        const uint8_t *entry = partition_data +
            (size_t)i * header.PartitionEntrySize;
        const struct GPT_PartitionEntry *partition =
            (const struct GPT_PartitionEntry *)entry;
        bool unused = true;
        for (unsigned int j = 0; j < 16; ++j)
            unused &= partition->PartitionTypeGUID[j] == 0;
        if (unused)
            continue;
        if (partition->EndLBA < partition->StartLBA ||
            partition->StartLBA < header.FirstUsableGptEntryBlock ||
            partition->EndLBA > header.LastUsableGptEntryBlock ||
            partition->EndLBA >= target->block_count)
            continue;
        msc_add_partition(target, partition->StartLBA,
            partition->EndLBA - partition->StartLBA + 1U, PART_TABLE_GPT,
            memcmp(partition->PartitionTypeGUID,
                "\x28\x73\x2A\xC1\x1F\xF8\xD2\x11\xBA\x4B\x00\xA0\xC9\x3E\xC9\x3B",
                16) == 0,
            0, partition->PartitionTypeGUID, ++ordinal);
    }
    kfree(partition_data);
    result = 0;

done:
    kfree(header_sector);
    return result;
}

static void msc_parse_mbr(usb_msc_target_t *target, const uint8_t *mbr) {
    if (mbr[510] != 0x55U || mbr[511] != 0xAAU)
        return;
    const mbr_partition_t *entries = (const mbr_partition_t *)(mbr + 446);
    unsigned int logical_ordinal = 5;
    for (unsigned int i = 0; i < 4; ++i) {
        if (!entries[i].partition_type || !entries[i].num_sectors ||
            entries[i].partition_type == 0xEEU)
            continue;
        if (entries[i].partition_type == 0x05U ||
            entries[i].partition_type == 0x0FU ||
            entries[i].partition_type == 0x85U)
            continue;
        msc_add_partition(target, entries[i].lba_start,
            entries[i].num_sectors, PART_TABLE_MBR,
            entries[i].boot_flag == MBR_PART_BOOTABLE,
            entries[i].partition_type, NULL, i + 1U);
    }

    uint8_t *ebr_sector = kmalloc_aligned(target->block_size, 16);
    if (!ebr_sector)
        return;
    uint64_t visited[USB_MSC_MAX_GPT_ENTRIES];
    for (unsigned int i = 0; i < 4; ++i) {
        uint8_t type = entries[i].partition_type;
        if (type != 0x05U && type != 0x0FU && type != 0x85U)
            continue;
        uint64_t extended_base = entries[i].lba_start;
        uint64_t extended_count = entries[i].num_sectors;
        if (extended_base >= target->block_count ||
            extended_count > target->block_count - extended_base)
            continue;
        uint64_t extended_end = extended_base + extended_count;
        uint64_t ebr_lba = extended_base;
        size_t visited_count = 0;
        for (size_t chain = 0; chain < USB_MSC_MAX_GPT_ENTRIES; ++chain) {
            bool repeated = false;
            for (size_t j = 0; j < visited_count; ++j)
                repeated |= visited[j] == ebr_lba;
            if (repeated || ebr_lba < extended_base ||
                ebr_lba >= extended_end ||
                block_read_sector(target->block_id, ebr_lba, ebr_sector, 1) != 0 ||
                ebr_sector[510] != 0x55U || ebr_sector[511] != 0xAAU)
                break;
            visited[visited_count++] = ebr_lba;

            const mbr_partition_t *logical =
                (const mbr_partition_t *)(ebr_sector + 446);
            if (logical[0].partition_type && logical[0].num_sectors &&
                logical[0].partition_type != 0x05U &&
                logical[0].partition_type != 0x0FU &&
                logical[0].partition_type != 0x85U &&
                ebr_lba <= UINT64_MAX - logical[0].lba_start &&
                ebr_lba + logical[0].lba_start < extended_end &&
                logical[0].num_sectors <= extended_end -
                    (ebr_lba + logical[0].lba_start)) {
                msc_add_partition(target, ebr_lba + logical[0].lba_start,
                    logical[0].num_sectors, PART_TABLE_MBR,
                    logical[0].boot_flag == MBR_PART_BOOTABLE,
                    logical[0].partition_type, NULL, logical_ordinal++);
            }

            if (!logical[1].partition_type ||
                (logical[1].partition_type != 0x05U &&
                    logical[1].partition_type != 0x0FU &&
                    logical[1].partition_type != 0x85U) ||
                extended_base > UINT64_MAX - logical[1].lba_start)
                break;
            ebr_lba = extended_base + logical[1].lba_start;
            if (ebr_lba >= extended_end)
                break;
        }
    }
    kfree(ebr_sector);
}

static void msc_parse_partitions(usb_msc_target_t *target) {
    uint8_t *mbr = kmalloc_aligned(target->block_size, 16);
    if (!mbr)
        return;
    if (block_read_sector(target->block_id, 0, mbr, 1) == 0 &&
        msc_parse_gpt(target, mbr) != 0)
        msc_parse_mbr(target, mbr);
    kfree(mbr);
}

static int msc_refresh_media(usb_msc_target_t *target) {
    if (msc_wait_ready(target) != 0) {
        target->media_present = 0;
        target->block_count = 0;
        block_device_info_t *dev = block_get_device(target->block_id);
        if (dev)
            dev->total_sectors = 0;
        return -1;
    }
    return msc_read_capacity(target);
}

static int msc_rw_blocks(usb_msc_target_t *target, uint64_t lba,
    uint32_t count, void *buffer, bool write) {
    if (!target || !buffer || count == 0 || !target->active)
        return -1;
    msc_lock(target->transport);
    int result = -1;
    if (!target->media_present) {
        if (msc_refresh_media(target) != 0)
            goto done;
        if (!target->partitions_scanned) {
            target->partitions_scanned = 1;
            msc_unlock(target->transport);
            msc_parse_partitions(target);
            msc_lock(target->transport);
            if (!target->active || !target->media_present)
                goto done;
        }
    }
    if (lba >= target->block_count ||
        (uint64_t)count > target->block_count - lba ||
        lba > UINT32_MAX ||
        (uint64_t)count * target->block_size > SIZE_MAX)
        goto done;

    uint64_t blocks_left = count;
    uint64_t current_lba = lba;
    uint8_t *out = buffer;
    while (blocks_left) {
        uint64_t max_blocks = USB_MSC_MAX_TRANSFER / target->block_size;
        if (max_blocks == 0)
            goto done;
        if (max_blocks > UINT16_MAX)
            max_blocks = UINT16_MAX;
        uint32_t blocks = blocks_left > max_blocks ?
            (uint32_t)max_blocks : (uint32_t)blocks_left;
        if (current_lba > UINT32_MAX ||
            (uint64_t)(blocks - 1U) > UINT32_MAX - current_lba)
            goto done;
        uint32_t bytes = blocks * target->block_size;
        uint8_t *dma = kmalloc_aligned(bytes, 16);
        if (!dma)
            goto done;
        uint8_t cdb[10] = {write ? 0x2AU : 0x28U, 0};
        cdb[2] = (uint8_t)(current_lba >> 24);
        cdb[3] = (uint8_t)(current_lba >> 16);
        cdb[4] = (uint8_t)(current_lba >> 8);
        cdb[5] = (uint8_t)current_lba;
        cdb[7] = (uint8_t)(blocks >> 8);
        cdb[8] = (uint8_t)blocks;
        if (write)
            memcpy(dma, out, bytes);
        uint32_t actual = 0;
        int command_result = msc_command(target, cdb, sizeof(cdb),
            write ? 0 : 0x80U, dma, bytes, &actual);
        if (command_result != 0 || actual != bytes) {
            if (command_result == 1) {
                uint8_t key = 0, asc = 0, ascq = 0;
                (void)msc_request_sense(target, &key, &asc, &ascq);
                if (key == 0x02U && asc == 0x3AU) {
                    target->media_present = 0;
                    target->block_count = 0;
                    block_device_info_t *dev = block_get_device(target->block_id);
                    if (dev)
                        dev->total_sectors = 0;
                }
            }
            kfree(dma);
            goto done;
        }
        if (!write)
            memcpy(out, dma, bytes);
        kfree(dma);
        out += bytes;
        current_lba += blocks;
        blocks_left -= blocks;
    }
    result = 0;

done:
    msc_unlock(target->transport);
    return result;
}

int usb_msc_read_blocks(int backend_index, uint64_t lba, uint32_t count,
    void *buffer) {
    if (backend_index < 0 || backend_index >= (int)USB_MSC_MAX_TARGETS)
        return -1;
    return msc_rw_blocks(&targets[backend_index], lba, count, buffer, false);
}

int usb_msc_write_blocks(int backend_index, uint64_t lba, uint32_t count,
    const void *buffer) {
    if (backend_index < 0 || backend_index >= (int)USB_MSC_MAX_TARGETS)
        return -1;
    return msc_rw_blocks(&targets[backend_index], lba, count,
        (void *)buffer, true);
}

int usb_msc_refresh_device(int backend_index) {
    if (backend_index < 0 || backend_index >= (int)USB_MSC_MAX_TARGETS)
        return -1;
    usb_msc_target_t *target = &targets[backend_index];
    if (!target->active || !target->transport)
        return -1;
    msc_lock(target->transport);
    int result = target->media_present ? 0 : msc_refresh_media(target);
    bool scan_partitions = result == 0 && !target->partitions_scanned;
    if (scan_partitions)
        target->partitions_scanned = 1;
    msc_unlock(target->transport);
    if (scan_partitions)
        msc_parse_partitions(target);
    return result;
}

static usb_msc_transport_t *msc_allocate_transport(void) {
    for (size_t i = 0; i < USB_MSC_MAX_TARGETS; ++i) {
        if (!transports[i].active)
            return &transports[i];
    }
    return NULL;
}

static usb_msc_target_t *msc_allocate_target(void) {
    for (size_t i = 0; i < USB_MSC_MAX_TARGETS; ++i) {
        if (!targets[i].active)
            return &targets[i];
    }
    return NULL;
}

static void msc_disconnect(usb_device_t *device) {
    for (size_t i = 0; i < USB_MSC_MAX_TARGETS; ++i) {
        usb_msc_target_t *target = &targets[i];
        if (!target->active || !target->transport ||
            target->transport->device != device)
            continue;
        (void)block_unregister_device(target->block_id);
        for (int p = general_partition_count - 1; p >= 0; --p) {
            general_partition_t *part = &ahci_partitions[p];
            if (part->ahci_port != (uint64_t)target->block_id)
                continue;
            for (int m = mounted_partition_count - 1; m >= 0; --m) {
                if (strcmp(mounted_partitions[m].part_name, part->name) == 0 &&
                    strcmp(mounted_partitions[m].mount_point, "/") != 0) {
                    char mount_point[256];
                    strncpy(mount_point, mounted_partitions[m].mount_point,
                        sizeof(mount_point) - 1);
                    mount_point[sizeof(mount_point) - 1] = '\0';
                    (void)vfs_umount(mount_point, true);
                }
            }
            for (int move = p; move < general_partition_count - 1; ++move)
                ahci_partitions[move] = ahci_partitions[move + 1];
            --general_partition_count;
            memset(&ahci_partitions[general_partition_count], 0,
                sizeof(ahci_partitions[general_partition_count]));
        }
        target->active = 0;
        target->media_present = 0;
    }
    for (size_t i = 0; i < USB_MSC_MAX_TARGETS; ++i) {
        if (transports[i].active && transports[i].device == device)
            transports[i].active = 0;
    }
}

static void msc_interface_callback(usb_device_t *device,
    const usb_interface_t *interface, bool connected) {
    if (!device || !interface)
        return;
    if (!connected) {
        msc_disconnect(device);
        return;
    }

    usb_msc_transport_t *transport = msc_allocate_transport();
    if (!transport)
        return;
    memset(transport, 0, sizeof(*transport));
    transport->device = device;
    transport->interface_number = interface->number;
    for (uint8_t i = 0; i < device->endpoint_count; ++i) {
        const usb_endpoint_t *endpoint = &device->endpoints[i];
        if (endpoint->interface_number != interface->number ||
            (endpoint->attributes & 3U) != 2U)
            continue;
        if (endpoint->address & 0x80U) {
            transport->in_address = endpoint->address;
            transport->in_endpoint = endpoint->endpoint_id;
        } else {
            transport->out_address = endpoint->address;
            transport->out_endpoint = endpoint->endpoint_id;
        }
    }
    if (!transport->in_endpoint || !transport->out_endpoint) {
        warn("USB mass-storage interface %u lacks bulk IN/OUT endpoints",
            __FILE__, interface->number);
        return;
    }
    transport->active = 1;
    uint8_t max_lun = 0;
    if (usb_control_request(device, 0xA1U, USB_MSC_GET_MAX_LUN, 0,
            interface->number, &max_lun, sizeof(max_lun)) != 0) {
        max_lun = 0;
    } else if (max_lun >= USB_MSC_MAX_LUNS) {
        warn("Ignoring invalid USB Mass Storage Max LUN value %u", __FILE__,
            max_lun);
        transport->active = 0;
        return;
    }
    transport->max_lun = max_lun;

    for (uint8_t lun = 0; lun <= max_lun; ++lun) {
        usb_msc_target_t *target = msc_allocate_target();
        if (!target)
            break;
        memset(target, 0, sizeof(*target));
        target->transport = transport;
        target->lun = lun;
        target->block_size = 512;
        target->active = 1;
        uint32_t disk_number = next_usb_disk_number++;
        char name[32];
        snprintf(name, sizeof(name), "usb%u", disk_number);
        target->block_id = block_register_device(BLOCK_DEVICE_USB,
            (int)(target - targets), 0, target->block_size, name);
        if (target->block_id < 0) {
            target->active = 0;
            continue;
        }

        bool scan_partitions = false;
        msc_lock(transport);
        if (msc_inquiry(target) != 0)
            warn("USB SCSI INQUIRY failed on LUN %u", __FILE__, lun);
        if (msc_wait_ready(target) == 0 && msc_read_capacity(target) == 0) {
            target->partitions_scanned = 1;
            scan_partitions = true;
        }
        msc_unlock(transport);
        if (scan_partitions)
            msc_parse_partitions(target);
        info("USB mass-storage LUN %u registered as %s (%u blocks)",
            __FILE__, lun, name, (unsigned long long)target->block_count);
    }
}

int usb_mass_storage_init(void) {
    return usb_register_class_driver(USB_MSC_CLASS,
        USB_MSC_SCSI_TRANSPARENT, USB_MSC_BULK_ONLY,
        msc_interface_callback);
}
