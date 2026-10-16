
/* ============================================================================
 * SYSFS COMPATIBILITY LAYER
 *
 * Toybox probes /sys/bus/pci/devices when discovering PCI devices. FrostWing
 * already exposes PCI details through procfs, so sysfs keeps a tiny read-only
 * mirror at the Linux-compatible PCI path.
 *
 * The static part of that tree (bus/, bus/pci/, bus/pci/devices/) is just
 * regular entries served by the same shared namespace engine procfs uses.
 * Device directories and attributes are generated on demand for PCI, USB,
 * block, and physical-memory entries.
 * ==========================================================================*/
#include <basics.h>
#include <filesystems/layers/namespace.h>
#include <filesystems/layers/proc.h>
#include <filesystems/layers/sys.h>
#include <pci.h>
#include <memory.h>
#include <paging.h>
#include <usb.h>

#define SYSFS_STATIC_FILES 11
#define SYSFS_MEMORY_BLOCK_SIZE (1 MiB)

/* Attribute files that live inside bus/pci/devices/<addr>/ */
enum { PCI_ATTR_VENDOR,
    PCI_ATTR_DEVICE,
    PCI_ATTR_CLASS,
    PCI_ATTR_REVISION,
    PCI_ATTR_UEVENT,
    PCI_ATTR_COUNT };

enum {
    USB_ATTR_VENDOR,
    USB_ATTR_PRODUCT,
    USB_ATTR_CLASS,
    USB_ATTR_SUBCLASS,
    USB_ATTR_PROTOCOL,
    USB_ATTR_BUSNUM,
    USB_ATTR_DEVNUM,
    USB_ATTR_SPEED,
    USB_ATTR_UEVENT,
    USB_ATTR_COUNT
};

static const char *pci_attr_names[PCI_ATTR_COUNT] = {
    "vendor", "device", "class", "revision", "uevent"};
static const char *usb_attr_names[USB_ATTR_COUNT] = {
    "idVendor", "idProduct", "bDeviceClass", "bDeviceSubClass",
    "bDeviceProtocol", "busnum", "devnum", "speed", "uevent"};

typedef struct {
    int index;
    int attr;
} sysfs_pci_priv_t;

/* If pci.h doesn't already declare these, keep these lines */
extern uint16 vendors[];
extern uint16 devices[];
extern uint16 classes[];
extern uint16 subclasses[];
extern uint8 revisions[];

static procfs_entry_t *sys_static_files[SYSFS_STATIC_FILES];
static int sys_static_file_count = 0;
static procfs_entry_t sys_pci_entry;
static sysfs_pci_priv_t sys_pci_priv;
static char sys_pci_entry_name[256];

static procfs_entry_t sys_bus = {
    .name = "bus",
    .type = PROC_DIR,
};

static procfs_entry_t sys_dev = {
    .name = "dev",
    .type = PROC_DIR,
};

static procfs_entry_t sys_dev_block = {
    .name = "dev/block",
    .type = PROC_DIR,
};

static procfs_entry_t sys_block = {
    .name = "block",
    .type = PROC_DIR,
};

static procfs_entry_t sys_bus_pci = {
    .name = "bus/pci",
    .type = PROC_DIR,
};

static procfs_entry_t sys_bus_pci_devices_dir = {
    .name = "bus/pci/devices",
    .type = PROC_DIR,
};

static procfs_entry_t sys_bus_usb = {
    .name = "bus/usb",
    .type = PROC_DIR,
};

static procfs_entry_t sys_bus_usb_devices_dir = {
    .name = "bus/usb/devices",
    .type = PROC_DIR,
};

static procfs_entry_t sys_devices = {
    .name = "devices",
    .type = PROC_DIR,
};

static procfs_entry_t sys_devices_system = {
    .name = "devices/system",
    .type = PROC_DIR,
};

static procfs_entry_t sys_devices_system_memory = {
    .name = "devices/system/memory",
    .type = PROC_DIR,
};

void sysfs_init(void) {
    sys_static_file_count = 0;
    memset(sys_static_files, 0, sizeof(sys_static_files));
    sys_static_files[sys_static_file_count++] = &sys_bus;
    sys_static_files[sys_static_file_count++] = &sys_dev;
    sys_static_files[sys_static_file_count++] = &sys_dev_block;
    sys_static_files[sys_static_file_count++] = &sys_block;
    sys_static_files[sys_static_file_count++] = &sys_bus_pci;
    sys_static_files[sys_static_file_count++] = &sys_bus_pci_devices_dir;
    sys_static_files[sys_static_file_count++] = &sys_bus_usb;
    sys_static_files[sys_static_file_count++] = &sys_bus_usb_devices_dir;
    sys_static_files[sys_static_file_count++] = &sys_devices;
    sys_static_files[sys_static_file_count++] = &sys_devices_system;
    sys_static_files[sys_static_file_count++] = &sys_devices_system_memory;
}

static void sysfs_pci_name(int i, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%04x:%02x:%02x.%x",
        0,
        pciLocations[i].bus,
        pciLocations[i].slot,
        pciLocations[i].func);
}

static int sysfs_pci_index_from_name(const char *name) {
    char tmp[32];
    for (int i = 0; i < total_devices; i++) {
        sysfs_pci_name(i, tmp, sizeof(tmp));
        if (strcmp(name, tmp) == 0)
            return i;
    }
    return -1;
}

static int sysfs_pci_attr_from_name(const char *name) {
    for (int a = 0; a < PCI_ATTR_COUNT; a++) {
        if (strcmp(name, pci_attr_names[a]) == 0)
            return a;
    }
    return -1;
}

static int sysfs_usb_index_from_name(const char *name) {
    char expected[32];
    for (size_t i = 0; i < usb_device_count(); ++i) {
        usb_device_t *device = usb_get_device(i);
        if (!device)
            continue;
        snprintf(expected, sizeof(expected), "1-%u", device->root_port);
        if (strcmp(name, expected) == 0)
            return (int)i;
    }
    return -1;
}

static int sysfs_usb_attr_read(vfs_file_t *file, uint8_t *buf,
    uint32_t size, void *priv) {
    sysfs_pci_priv_t *p = priv;
    usb_device_t *device = usb_get_device((size_t)p->index);
    if (!device || p->attr < 0 || p->attr >= USB_ATTR_COUNT)
        return -1;

    char tmp[128];
    int len;
    switch (p->attr) {
        case USB_ATTR_VENDOR:
            len = snprintf(tmp, sizeof(tmp), "%04x\n", device->descriptor.vendor_id);
            break;
        case USB_ATTR_PRODUCT:
            len = snprintf(tmp, sizeof(tmp), "%04x\n", device->descriptor.product_id);
            break;
        case USB_ATTR_CLASS:
            len = snprintf(tmp, sizeof(tmp), "%02x\n", device->descriptor.device_class);
            break;
        case USB_ATTR_SUBCLASS:
            len = snprintf(tmp, sizeof(tmp), "%02x\n", device->descriptor.device_subclass);
            break;
        case USB_ATTR_PROTOCOL:
            len = snprintf(tmp, sizeof(tmp), "%02x\n", device->descriptor.device_protocol);
            break;
        case USB_ATTR_BUSNUM:
            len = snprintf(tmp, sizeof(tmp), "001\n");
            break;
        case USB_ATTR_DEVNUM:
            len = snprintf(tmp, sizeof(tmp), "%03u\n", device->slot_id);
            break;
        case USB_ATTR_SPEED:
            switch (device->speed) {
                case 1:
                    len = snprintf(tmp, sizeof(tmp), "12\n");
                    break;
                case 2:
                    len = snprintf(tmp, sizeof(tmp), "1.5\n");
                    break;
                case 3:
                    len = snprintf(tmp, sizeof(tmp), "480\n");
                    break;
                case 4:
                    len = snprintf(tmp, sizeof(tmp), "5000\n");
                    break;
                case 5:
                    len = snprintf(tmp, sizeof(tmp), "10000\n");
                    break;
                default:
                    len = snprintf(tmp, sizeof(tmp), "unknown\n");
                    break;
            }
            break;
        case USB_ATTR_UEVENT:
            len = snprintf(tmp, sizeof(tmp),
                "DEVTYPE=usb_device\nBUSNUM=001\nDEVNUM=%03u\n"
                "PRODUCT=%04x/%04x/%x\n",
                device->slot_id, device->descriptor.vendor_id,
                device->descriptor.product_id, device->descriptor.device_version);
            break;
        default:
            return -1;
    }
    return ns_reply(file, buf, size, tmp, len);
}

/* Reads one attribute file (vendor, device, ...) of one PCI device. */
static int sysfs_pci_attr_read(vfs_file_t *file, uint8_t *buf, uint32_t size, void *priv) {
    sysfs_pci_priv_t *p = priv;
    int i = p->index;
    char tmp[256];
    int len = 0;

    switch (p->attr) {
        case PCI_ATTR_VENDOR:
            len = snprintf(tmp, sizeof(tmp), "0x%04x\n", vendors[i]);
            break;
        case PCI_ATTR_DEVICE:
            len = snprintf(tmp, sizeof(tmp), "0x%04x\n", devices[i]);
            break;
        case PCI_ATTR_CLASS:
            /* Linux format: 0xCCSSPP (class, subclass, prog-if). prog-if isn't stored, so 00. */
            len = snprintf(tmp, sizeof(tmp), "0x%02x%02x00\n", classes[i], subclasses[i]);
            break;
        case PCI_ATTR_REVISION:
            len = snprintf(tmp, sizeof(tmp), "0x%02x\n", revisions[i]);
            break;
        case PCI_ATTR_UEVENT:
            len = snprintf(tmp, sizeof(tmp),
                "PCI_CLASS=%02X%02X00\n"
                "PCI_ID=%04X:%04X\n"
                "PCI_SLOT_NAME=%04x:%02x:%02x.%x\n",
                classes[i], subclasses[i],
                vendors[i], devices[i],
                0, pciLocations[i].bus, pciLocations[i].slot, pciLocations[i].func);
            break;
        default:
            return -1;
    }

    if (len < 0)
        return -1;
    if (len > (int)sizeof(tmp) - 1)
        len = sizeof(tmp) - 1;

    if (file->pos >= (uint32_t)len)
        return 0;

    uint32_t rem = len - file->pos;
    if (rem > size)
        rem = size;

    memcpy(buf, tmp + file->pos, rem);
    file->pos += rem;
    return (int)rem;
}

/*
 * Parses "bus/pci/devices/<addr>[/<attr>]".
 * Returns  0 if the path isn't under bus/pci/devices/ (or is the devices dir itself)
 *          1 if it resolved; *index = device index, *attr = PCI_ATTR_* or -1 for the device dir
 *         -1 if it's under devices/ but names something that doesn't exist
 */
static int sysfs_pci_parse(const char *normalized, int *index, int *attr) {
    static const char prefix[] = "bus/pci/devices/";
    size_t prefix_len = sizeof(prefix) - 1;

    if (strncmp(normalized, prefix, prefix_len) != 0)
        return 0;

    const char *rest = normalized + prefix_len;
    if (*rest == '\0')
        return 0;

    char addr[32];
    const char *slash = strchr(rest, '/');
    size_t alen = slash ? (size_t)(slash - rest) : (size_t)strlen(rest);
    if (alen == 0 || alen >= sizeof(addr))
        return -1;

    memcpy(addr, rest, alen);
    addr[alen] = '\0';

    int idx = sysfs_pci_index_from_name(addr);
    if (idx < 0)
        return -1;

    *index = idx;

    if (!slash || slash[1] == '\0') {
        *attr = -1; /* the device directory itself */
        return 1;
    }

    int a = sysfs_pci_attr_from_name(slash + 1);
    if (a < 0)
        return -1;

    *attr = a;
    return 1;
}

static int sysfs_block_count(void) {
    int count = 0;
    for (int i = 0; i < block_device_count; i++) {
        if (block_devices[i].present)
            count++;
    }
    for (int i = 0; i < general_partition_count; i++) {
        if (block_get_device((int)ahci_partitions[i].ahci_port))
            count++;
    }
    return count;
}

static int sysfs_block_disk_id(const char *name) {
    for (int i = 0; i < block_device_count; i++) {
        if (block_devices[i].present && strcmp(block_devices[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int sysfs_block_partition_index(const char *name, int disk_id) {
    for (int i = 0; i < general_partition_count; i++) {
        if ((int)ahci_partitions[i].ahci_port == disk_id &&
            strcmp(ahci_partitions[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int sysfs_block_get_entry(int index, int *major, int *minor) {
    int count = 0;

    for (int i = 0; i < block_device_count; i++) {
        if (!block_devices[i].present)
            continue;
        if (count == index) {
            *major = (block_devices[i].type == BLOCK_DEVICE_NVME) ? 259 : 8;
            *minor = i * 16;
            return 1;
        }
        count++;
    }

    for (int i = 0; i < general_partition_count; i++) {
        if (!block_get_device((int)ahci_partitions[i].ahci_port))
            continue;
        if (count == index) {
            int device_id = (int)ahci_partitions[i].ahci_port;
            int device_major = (block_devices[device_id].type == BLOCK_DEVICE_NVME) ? 259 : 8;
            int part_minor = 1;
            for (int j = 0; j < i; j++) {
                if (ahci_partitions[j].ahci_port == ahci_partitions[i].ahci_port)
                    part_minor++;
            }
            *major = device_major;
            *minor = device_id * 16 + part_minor;
            return 1;
        }
        count++;
    }

    return 0;
}

static int sysfs_block_partition_minor(int partition_index) {
    int disk_id = (int)ahci_partitions[partition_index].ahci_port;
    int minor = 1;
    for (int i = 0; i < partition_index; i++) {
        if (ahci_partitions[i].ahci_port == ahci_partitions[partition_index].ahci_port)
            minor++;
    }
    return disk_id * 16 + minor;
}

static int sysfs_block_dir_kind(const char *path, int *disk_id, int *partition_index) {
    if (strcmp(path, "block") == 0)
        return 1;

    if (strncmp(path, "block/", 6) != 0)
        return 0;

    const char *name = path + 6;
    const char *slash = strchr(name, '/');
    size_t len = slash ? (size_t)(slash - name) : (size_t)strlen(name);
    if (len == 0 || len >= 64)
        return 0;

    char disk_name[64];
    memcpy(disk_name, name, len);
    disk_name[len] = '\0';
    int disk = sysfs_block_disk_id(disk_name);
    if (disk < 0)
        return 0;

    if (!slash) {
        if (disk_id)
            *disk_id = disk;
        return 2;
    }

    const char *child = slash + 1;
    if (strcmp(child, "queue") == 0) {
        if (disk_id)
            *disk_id = disk;
        return 3;
    }

    if (strchr(child, '/') != NULL)
        return 0;

    int partition = sysfs_block_partition_index(child, disk);
    if (partition < 0)
        return 0;
    if (disk_id)
        *disk_id = disk;
    if (partition_index)
        *partition_index = partition;
    return 4;
}

static int sysfs_block_resolve(const char *path, char *resolved, size_t capacity);

static int sysfs_block_get_attr(const char *path, char *contents, size_t capacity) {
    char resolved[256];
    int resolve_result = sysfs_block_resolve(path, resolved, sizeof(resolved));
    if (resolve_result < 0)
        return -1;
    if (resolve_result == 1)
        path = resolved;

    int disk_id = -1;
    int partition_index = -1;
    const char *slash = strrchr(path, '/');
    if (!slash || !slash[1])
        return -1;

    char parent[256];
    size_t parent_len = (size_t)(slash - path);
    if (parent_len >= sizeof(parent))
        return -1;
    memcpy(parent, path, parent_len);
    parent[parent_len] = '\0';

    int kind = sysfs_block_dir_kind(parent, &disk_id, &partition_index);
    if (kind != 2 && kind != 4 && kind != 3)
        return -1;

    const char *attr = slash + 1;

    if (kind == 3) {
        uint32_t sector_size = block_devices[disk_id].sector_size;
        if (strcmp(attr, "logical_block_size") == 0 ||
            strcmp(attr, "physical_block_size") == 0)
            return snprintf(contents, capacity, "%u\n", sector_size);
        return -1;
    }

    block_device_info_t *device = &block_devices[disk_id];
    int major = device->type == BLOCK_DEVICE_NVME ? 259 : 8;
    int minor = disk_id * 16;
    uint64_t sectors = device->total_sectors;
    const char *name = device->name;
    const char *device_type = "disk";
    uint64_t start = 0;

    if (kind == 4) {
        general_partition_t *partition = &ahci_partitions[partition_index];
        minor = sysfs_block_partition_minor(partition_index);
        sectors = partition->sector_count;
        name = partition->name;
        device_type = "partition";
        start = partition->lba_start;
    }

    if (strcmp(attr, "dev") == 0)
        return snprintf(contents, capacity, "%d:%d\n", major, minor);
    if (strcmp(attr, "size") == 0) {
        uint64_t factor = device->sector_size / 512;
        return snprintf(contents, capacity, "%llu\n",
            (unsigned long long)(sectors * factor));
    }
    if (strcmp(attr, "ro") == 0 || strcmp(attr, "removable") == 0)
        return snprintf(contents, capacity, "0\n");
    if (kind == 4 && strcmp(attr, "partition") == 0) {
        unsigned int number = 1;
        for (int i = 0; i < partition_index; i++) {
            if (ahci_partitions[i].ahci_port == (uint64)disk_id)
                number++;
        }
        return snprintf(contents, capacity, "%u\n", number);
    }
    if (kind == 4 && strcmp(attr, "start") == 0)
        return snprintf(contents, capacity, "%llu\n",
            (unsigned long long)start);
    if (strcmp(attr, "uevent") == 0)
        return snprintf(contents, capacity,
            "MAJOR=%d\nMINOR=%d\nDEVNAME=%s\nDEVTYPE=%s\n",
            major, minor, name, device_type);

    return -1;
}

static int sysfs_block_attr_read(vfs_file_t *file, uint8_t *buf, uint32_t size, void *priv) {
    (void)priv;
    char contents[192];
    int len = sysfs_block_get_attr(file->rel_path, contents, sizeof(contents));
    if (len < 0)
        return -1;
    return ns_reply(file, buf, size, contents, len);
}

static int sysfs_parse_decimal(const char *s) {
    int value = 0;
    if (!s || !*s)
        return -1;
    while (*s >= '0' && *s <= '9') {
        int digit = *s - '0';
        if (value > (2147483647 - digit) / 10)
            return -1;
        value = value * 10 + digit;
        s++;
    }
    if (*s != '\0')
        return -1;
    return value;
}

static int sysfs_parse_u64(const char *s, uint64_t *value, const char **end) {
    uint64_t parsed = 0;
    const char *p = s;
    if (!p || *p < '0' || *p > '9')
        return 0;

    do {
        uint32_t digit = (uint32_t)(*p - '0');
        if (parsed > (UINT64_MAX - digit) / 10)
            return 0;
        parsed = parsed * 10 + digit;
        p++;
    } while (*p >= '0' && *p <= '9');

    *value = parsed;
    if (end)
        *end = p;
    return 1;
}

static int sysfs_memory_range(const struct limine_memmap_entry *entry,
    uint64_t *first, uint64_t *end) {
    const uint64_t block_size = SYSFS_MEMORY_BLOCK_SIZE;
    if (entry->type != LIMINE_MEMMAP_USABLE ||
        entry->base > UINT64_MAX - entry->length)
        return 0;

    uint64_t range_end = entry->base + entry->length;
    if (entry->base > UINT64_MAX - (block_size - 1))
        return 0;
    uint64_t aligned_start = (entry->base + block_size - 1) & ~(block_size - 1);
    uint64_t aligned_end = range_end & ~(block_size - 1);
    if (aligned_end <= aligned_start)
        return 0;

    *first = aligned_start / block_size;
    *end = aligned_end / block_size;
    return 1;
}

static int sysfs_memory_get_block(uint64_t ordinal, uint64_t *phys_index) {
    if (!memmap || !phys_index)
        return 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        uint64_t first, end;
        if (!sysfs_memory_range(memmap->entries[i], &first, &end))
            continue;
        uint64_t count = end - first;
        if (ordinal < count) {
            *phys_index = first + ordinal;
            return 1;
        }
        ordinal -= count;
    }
    return 0;
}

static uint64_t sysfs_memory_block_count(void) {
    uint64_t count = 0;
    if (!memmap)
        return 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        uint64_t first, end;
        if (!sysfs_memory_range(memmap->entries[i], &first, &end))
            continue;
        uint64_t range_count = end - first;
        if (count > UINT64_MAX - range_count)
            return UINT64_MAX;
        count += range_count;
    }
    return count;
}

static int sysfs_memory_has_block(uint64_t phys_index) {
    if (!memmap)
        return 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        uint64_t first, end;
        if (sysfs_memory_range(memmap->entries[i], &first, &end) &&
            phys_index >= first && phys_index < end)
            return 1;
    }
    return 0;
}

static int sysfs_memory_parse(const char *path, uint64_t *phys_index,
    const char **attr_name) {
    static const char prefix[] = "devices/system/memory/memory";
    if (strncmp(path, prefix, sizeof(prefix) - 1) != 0)
        return 0;

    const char *number = path + sizeof(prefix) - 1;
    const char *end;
    uint64_t index;
    if (!sysfs_parse_u64(number, &index, &end) || !sysfs_memory_has_block(index))
        return -1;
    if (*end == '\0') {
        if (phys_index)
            *phys_index = index;
        if (attr_name)
            *attr_name = NULL;
        return 1;
    }
    if (*end != '/' || !end[1] || strchr(end + 1, '/'))
        return -1;

    static const char *const attributes[] = {
        "phys_index", "state", "valid_zones", "removable", "phys_device", "uevent"};
    for (size_t i = 0; i < sizeof(attributes) / sizeof(attributes[0]); i++) {
        if (strcmp(end + 1, attributes[i]) == 0) {
            if (phys_index)
                *phys_index = index;
            if (attr_name)
                *attr_name = attributes[i];
            return 1;
        }
    }
    return -1;
}

static int sysfs_memory_ranges(char *out, size_t capacity) {
    if (!out || capacity == 0)
        return -1;
    out[0] = '\0';
    if (!memmap)
        return 0;

    size_t used = 0;
    uint64_t pending_first = 0, pending_end = 0;
    int have_pending = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        uint64_t first, end;
        if (!sysfs_memory_range(memmap->entries[i], &first, &end))
            continue;
        if (have_pending && first <= pending_end) {
            if (end > pending_end)
                pending_end = end;
            continue;
        }
        if (have_pending) {
            int len;
            if (pending_end - pending_first == 1)
                len = snprintf(out + used, capacity - used, "%s%llu",
                    used ? "," : "", (unsigned long long)pending_first);
            else
                len = snprintf(out + used, capacity - used, "%s%llu-%llu",
                    used ? "," : "", (unsigned long long)pending_first,
                    (unsigned long long)(pending_end - 1));
            if (len < 0 || (size_t)len >= capacity - used)
                return -1;
            used += (size_t)len;
        }
        pending_first = first;
        pending_end = end;
        have_pending = 1;
    }
    if (have_pending) {
        int len;
        if (pending_end - pending_first == 1)
            len = snprintf(out + used, capacity - used, "%s%llu\n",
                used ? "," : "", (unsigned long long)pending_first);
        else
            len = snprintf(out + used, capacity - used, "%s%llu-%llu\n",
                used ? "," : "", (unsigned long long)pending_first,
                (unsigned long long)(pending_end - 1));
        if (len < 0 || (size_t)len >= capacity - used)
            return -1;
        used += (size_t)len;
    }
    return (int)used;
}

static int sysfs_memory_get_attr(const char *path, char *contents, size_t capacity) {
    static const char prefix[] = "devices/system/memory/";
    if (strncmp(path, prefix, sizeof(prefix) - 1) != 0)
        return -1;

    const char *name = path + sizeof(prefix) - 1;
    if (strchr(name, '/') == NULL) {
        if (strcmp(name, "block_size_bytes") == 0)
            return snprintf(contents, capacity, "%llx\n",
                (unsigned long long)SYSFS_MEMORY_BLOCK_SIZE);
        if (strcmp(name, "auto_online_blocks") == 0)
            return snprintf(contents, capacity, "offline\n");
        if (strcmp(name, "valid_zones") == 0)
            return snprintf(contents, capacity, "Normal\n");
        if (strcmp(name, "offline") == 0)
            return 0;
        if (strcmp(name, "online") == 0 || strcmp(name, "possible") == 0 ||
            strcmp(name, "present") == 0)
            return sysfs_memory_ranges(contents, capacity);
        return -1;
    }

    uint64_t phys_index;
    const char *attr_name;
    if (sysfs_memory_parse(path, &phys_index, &attr_name) != 1 || !attr_name)
        return -1;
    if (strcmp(attr_name, "phys_index") == 0)
        return snprintf(contents, capacity, "%llu\n",
            (unsigned long long)phys_index);
    if (strcmp(attr_name, "state") == 0)
        return snprintf(contents, capacity, "online\n");
    if (strcmp(attr_name, "valid_zones") == 0)
        return snprintf(contents, capacity, "Normal\n");
    if (strcmp(attr_name, "removable") == 0 ||
        strcmp(attr_name, "phys_device") == 0)
        return snprintf(contents, capacity, "0\n");
    if (strcmp(attr_name, "uevent") == 0)
        return snprintf(contents, capacity, "DEVTYPE=memory\n");
    return -1;
}

static int sysfs_memory_attr_read(vfs_file_t *file, uint8_t *buf,
    uint32_t size, void *priv) {
    (void)priv;
    char contents[2048];
    int len = sysfs_memory_get_attr(file->rel_path, contents, sizeof(contents));
    if (len < 0)
        return -1;
    return ns_reply(file, buf, size, contents, len);
}

static int sysfs_block_parse(const char *normalized, int *major, int *minor) {
    static const char prefix[] = "dev/block/";
    size_t prefix_len = sizeof(prefix) - 1;
    if (strncmp(normalized, prefix, prefix_len) != 0)
        return 0;

    const char *rest = normalized + prefix_len;
    if (!*rest)
        return 0;

    const char *colon = strchr(rest, ':');
    if (!colon || !colon[1])
        return -1;

    char major_buf[32];
    size_t major_len = (size_t)(colon - rest);
    if (major_len == 0 || major_len >= sizeof(major_buf))
        return -1;

    memcpy(major_buf, rest, major_len);
    major_buf[major_len] = '\0';
    *major = sysfs_parse_decimal(major_buf);
    *minor = sysfs_parse_decimal(colon + 1);
    if (*major < 0 || *minor < 0)
        return -1;

    for (int i = 0; i < sysfs_block_count(); i++) {
        int m = 0, n = 0;
        if (sysfs_block_get_entry(i, &m, &n) && m == *major && n == *minor)
            return 1;
    }
    return -1;
}

static int sysfs_block_resolve(const char *path, char *resolved, size_t capacity) {
    static const char prefix[] = "dev/block/";
    if (strncmp(path, prefix, sizeof(prefix) - 1) != 0)
        return 0;

    const char *rest = path + sizeof(prefix) - 1;
    const char *slash = strchr(rest, '/');
    size_t link_len = slash ? (size_t)(slash - rest) : (size_t)strlen(rest);
    if (link_len == 0 || link_len >= 32)
        return 0;

    char link_path[64];
    memcpy(link_path, prefix, sizeof(prefix) - 1);
    memcpy(link_path + sizeof(prefix) - 1, rest, link_len);
    link_path[sizeof(prefix) - 1 + link_len] = '\0';

    char target[192];
    int target_len = sysfs_readlink(link_path, target, sizeof(target));
    static const char relative_prefix[] = "../../";
    if (target_len < (int)(sizeof(relative_prefix) - 1) ||
        (size_t)target_len >= sizeof(target) ||
        memcmp(target, relative_prefix, sizeof(relative_prefix) - 1) != 0)
        return 0;
    target[target_len] = '\0';

    const char *suffix = slash ? slash : "";
    size_t resolved_len = (size_t)target_len - (sizeof(relative_prefix) - 1);
    size_t suffix_len = strlen(suffix);
    if (resolved_len + suffix_len >= capacity)
        return -1;

    memcpy(resolved, target + sizeof(relative_prefix) - 1, resolved_len);
    memcpy(resolved + resolved_len, suffix, suffix_len + 1);
    return 1;
}

static procfs_entry_t *sysfs_find_dynamic(const char *normalized) {
    if (strncmp(normalized, "dev/block/", 10) == 0) {
        const char *rest = normalized + 10;
        const char *slash = strchr(rest, '/');
        if (slash && slash[1]) {
            char resolved[256];
            int result = sysfs_block_resolve(normalized, resolved, sizeof(resolved));
            if (result == 1)
                return sysfs_find_dynamic(resolved);
            if (result < 0)
                return NULL;
        }
    }

    uint64_t memory_index;
    const char *memory_attr;
    if (sysfs_memory_parse(normalized, &memory_index, &memory_attr) == 1) {
        (void)memory_index;
        strncpy(sys_pci_entry_name, normalized, sizeof(sys_pci_entry_name) - 1);
        sys_pci_entry_name[sizeof(sys_pci_entry_name) - 1] = '\0';
        sys_pci_entry.name = sys_pci_entry_name;
        sys_pci_entry.type = memory_attr ? PROC_FILE : PROC_DIR;
        sys_pci_entry.read = memory_attr ? sysfs_memory_attr_read : NULL;
        sys_pci_entry.write = NULL;
        sys_pci_entry.priv = NULL;
        return &sys_pci_entry;
    }

    char memory_attr_contents[2048];
    if (sysfs_memory_get_attr(normalized, memory_attr_contents,
            sizeof(memory_attr_contents)) >= 0) {
        strncpy(sys_pci_entry_name, normalized, sizeof(sys_pci_entry_name) - 1);
        sys_pci_entry_name[sizeof(sys_pci_entry_name) - 1] = '\0';
        sys_pci_entry.name = sys_pci_entry_name;
        sys_pci_entry.type = PROC_FILE;
        sys_pci_entry.read = sysfs_memory_attr_read;
        sys_pci_entry.write = NULL;
        sys_pci_entry.priv = NULL;
        return &sys_pci_entry;
    }

    int disk_id = -1;
    int partition_index = -1;
    int dir_kind = sysfs_block_dir_kind(normalized, &disk_id, &partition_index);
    if (dir_kind != 0) {
        strncpy(sys_pci_entry_name, normalized, sizeof(sys_pci_entry_name) - 1);
        sys_pci_entry_name[sizeof(sys_pci_entry_name) - 1] = '\0';
        sys_pci_entry.name = sys_pci_entry_name;
        sys_pci_entry.type = PROC_DIR;
        sys_pci_entry.read = NULL;
        sys_pci_entry.write = NULL;
        sys_pci_entry.priv = NULL;
        return &sys_pci_entry;
    }

    char block_attr[192];
    if (sysfs_block_get_attr(normalized, block_attr, sizeof(block_attr)) >= 0) {
        strncpy(sys_pci_entry_name, normalized, sizeof(sys_pci_entry_name) - 1);
        sys_pci_entry_name[sizeof(sys_pci_entry_name) - 1] = '\0';
        sys_pci_entry.name = sys_pci_entry_name;
        sys_pci_entry.type = PROC_FILE;
        sys_pci_entry.read = sysfs_block_attr_read;
        sys_pci_entry.write = NULL;
        sys_pci_entry.priv = NULL;
        return &sys_pci_entry;
    }

    if (strcmp(normalized, "dev/block") == 0) {
        sys_pci_priv.index = 0;
        sys_pci_priv.attr = -1;
        strncpy(sys_pci_entry_name, normalized, sizeof(sys_pci_entry_name) - 1);
        sys_pci_entry_name[sizeof(sys_pci_entry_name) - 1] = '\0';
        sys_pci_entry.name = sys_pci_entry_name;
        sys_pci_entry.type = PROC_DIR;
        sys_pci_entry.read = NULL;
        sys_pci_entry.write = NULL;
        sys_pci_entry.priv = &sys_pci_priv;
        return &sys_pci_entry;
    }

    if (sysfs_block_parse(normalized, &sys_pci_priv.index, &sys_pci_priv.attr) == 1) {
        strncpy(sys_pci_entry_name, normalized, sizeof(sys_pci_entry_name) - 1);
        sys_pci_entry_name[sizeof(sys_pci_entry_name) - 1] = '\0';
        sys_pci_entry.name = sys_pci_entry_name;
        sys_pci_entry.type = PROC_SYMLINK;
        sys_pci_entry.read = NULL;
        sys_pci_entry.write = NULL;
        sys_pci_entry.priv = &sys_pci_priv;
        return &sys_pci_entry;
    }

    const char *usb_prefix = "bus/usb/devices/";
    size_t prefix_len = strlen(usb_prefix);
    if (strncmp(normalized, usb_prefix, prefix_len) == 0) {
        const char *rest = normalized + prefix_len;
        const char *slash = strchr(rest, '/');
        size_t name_len = slash ? (size_t)(slash - rest) : (size_t)strlen(rest);
        char name[32];
        if (name_len == 0 || name_len >= sizeof(name))
            return NULL;
        memcpy(name, rest, name_len);
        name[name_len] = '\0';
        int usb_index = sysfs_usb_index_from_name(name);
        if (usb_index >= 0) {
            int attr = -1;
            if (slash) {
                if (!slash[1])
                    return NULL;
                for (int i = 0; i < USB_ATTR_COUNT; ++i) {
                    if (strcmp(slash + 1, usb_attr_names[i]) == 0) {
                        attr = i;
                        break;
                    }
                }
                if (attr < 0)
                    return NULL;
            }
            sys_pci_priv.index = usb_index;
            sys_pci_priv.attr = attr;
            strncpy(sys_pci_entry_name, normalized,
                sizeof(sys_pci_entry_name) - 1);
            sys_pci_entry_name[sizeof(sys_pci_entry_name) - 1] = '\0';
            sys_pci_entry.name = sys_pci_entry_name;
            sys_pci_entry.type = attr < 0 ? PROC_DIR : PROC_FILE;
            sys_pci_entry.read = attr < 0 ? NULL : sysfs_usb_attr_read;
            sys_pci_entry.write = NULL;
            sys_pci_entry.priv = &sys_pci_priv;
            return &sys_pci_entry;
        }
        return NULL;
    }
    int index, attr;
    if (sysfs_pci_parse(normalized, &index, &attr) != 1)
        return NULL;

    sys_pci_priv.index = index;
    sys_pci_priv.attr = attr;
    /* Copy the name: `normalized` lives on the caller's stack. */
    strncpy(sys_pci_entry_name, normalized, sizeof(sys_pci_entry_name) - 1);
    sys_pci_entry_name[sizeof(sys_pci_entry_name) - 1] = '\0';

    sys_pci_entry.name = sys_pci_entry_name;
    sys_pci_entry.write = NULL;
    sys_pci_entry.priv = &sys_pci_priv;

    if (attr < 0) {
        sys_pci_entry.type = PROC_DIR;
        sys_pci_entry.read = NULL;
    } else {
        sys_pci_entry.type = PROC_FILE;
        sys_pci_entry.read = sysfs_pci_attr_read;
    }
    return &sys_pci_entry;
}

static procfs_entry_t *sysfs_find(const char *name) {
    char normalized[256];
    ns_normalize_path(name, normalized, sizeof(normalized));

    if (normalized[0] == '\0')
        return &sys_bus;

    procfs_entry_t *e = ns_find(sys_static_files, sys_static_file_count, normalized);
    if (e)
        return e;

    return sysfs_find_dynamic(normalized);
}

int sysfs_open(vfs_file_t *file) {
    if (!file)
        return -1;

    if (!sysfs_find(file->rel_path))
        return -1;

    file->pos = 0;
    return 0;
}

int sysfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size) {
    if (!file || !buf)
        return -1;

    procfs_entry_t *e = sysfs_find(file->rel_path);
    if (!e || !e->read)
        return -1;

    return e->read(file, buf, size, e->priv);
}

int sysfs_is_dir(const char *path) {
    procfs_entry_t *e = sysfs_find(path);
    return e && e->type == PROC_DIR;
}

int sysfs_is_symlink(const char *path) {
    procfs_entry_t *e = sysfs_find(path);
    return e && e->type == PROC_SYMLINK;
}

int sysfs_readlink(const char *path, char *buf, uint32_t bufsiz) {
    if (!path || !buf || bufsiz == 0)
        return -1;

    char normalized[256];
    ns_normalize_path(path, normalized, sizeof(normalized));
    int major, minor;
    if (sysfs_block_parse(normalized, &major, &minor) != 1)
        return -1;

    char target[192];
    for (int i = 0; i < block_device_count; i++) {
        block_device_info_t *device = &block_devices[i];
        if (!device->present)
            continue;
        int device_major = device->type == BLOCK_DEVICE_NVME ? 259 : 8;
        if (major == device_major && minor == i * 16) {
            int len = snprintf(target, sizeof(target), "../../block/%s", device->name);
            if (len < 0 || (size_t)len >= sizeof(target))
                return -1;
            uint32_t copied = (uint32_t)len < bufsiz ? (uint32_t)len : bufsiz;
            memcpy(buf, target, copied);
            return (int)copied;
        }
    }

    for (int i = 0; i < general_partition_count; i++) {
        int device_id = (int)ahci_partitions[i].ahci_port;
        block_device_info_t *device = block_get_device(device_id);
        if (!device)
            continue;
        int device_major = device->type == BLOCK_DEVICE_NVME ? 259 : 8;
        if (major == device_major && minor == sysfs_block_partition_minor(i)) {
            int len = snprintf(target, sizeof(target), "../../block/%s/%s",
                device->name, ahci_partitions[i].name);
            if (len < 0 || (size_t)len >= sizeof(target))
                return -1;
            uint32_t copied = (uint32_t)len < bufsiz ? (uint32_t)len : bufsiz;
            memcpy(buf, target, copied);
            return (int)copied;
        }
    }

    return -1;
}

int sysfs_getdent(const char *path, uint64_t index, const char **out_name, procfs_type_t *out_type) {
    char normalized[256];
    ns_normalize_path(path, normalized, sizeof(normalized));

    char resolved[256];
    int resolve_result = sysfs_block_resolve(normalized, resolved, sizeof(resolved));
    if (resolve_result < 0)
        return 0;
    if (resolve_result == 1)
        strcpy(normalized, resolved);

    static const char *const memory_root_attrs[] = {
        "auto_online_blocks", "block_size_bytes", "offline", "online",
        "possible", "present", "valid_zones"};
    if (strcmp(normalized, "devices/system/memory") == 0) {
        uint64_t block_count = sysfs_memory_block_count();
        if (index < block_count) {
            uint64_t phys_index;
            if (!sysfs_memory_get_block(index, &phys_index))
                return 0;
            static char memory_name[32];
            snprintf(memory_name, sizeof(memory_name), "memory%llu",
                (unsigned long long)phys_index);
            *out_name = memory_name;
            *out_type = PROC_DIR;
            return 1;
        }
        index -= block_count;
        if (index >= sizeof(memory_root_attrs) / sizeof(memory_root_attrs[0]))
            return 0;
        *out_name = memory_root_attrs[index];
        *out_type = PROC_FILE;
        return 1;
    }

    uint64_t memory_index;
    const char *memory_attr;
    if (sysfs_memory_parse(normalized, &memory_index, &memory_attr) == 1 &&
        !memory_attr) {
        static const char *const memory_block_attrs[] = {
            "phys_device", "phys_index", "removable", "state",
            "uevent", "valid_zones"};
        if (index >= sizeof(memory_block_attrs) / sizeof(memory_block_attrs[0]))
            return 0;
        *out_name = memory_block_attrs[index];
        *out_type = PROC_FILE;
        return 1;
    }

    int disk_id = -1;
    int partition_index = -1;
    int dir_kind = sysfs_block_dir_kind(normalized, &disk_id, &partition_index);
    if (dir_kind == 1) {
        uint64_t seen = 0;
        for (int i = 0; i < block_device_count; i++) {
            if (!block_devices[i].present)
                continue;
            if (seen++ != index)
                continue;
            *out_name = block_devices[i].name;
            *out_type = PROC_DIR;
            return 1;
        }
        return 0;
    }
    if (dir_kind == 2) {
        static const char *disk_attrs[] = {"queue", "dev", "size", "ro", "removable", "uevent"};
        uint64_t entry = 0;
        for (size_t i = 0; i < sizeof(disk_attrs) / sizeof(disk_attrs[0]); i++) {
            if (entry++ == index) {
                *out_name = disk_attrs[i];
                *out_type = strcmp(disk_attrs[i], "queue") == 0 ? PROC_DIR : PROC_FILE;
                return 1;
            }
        }
        for (int i = 0; i < general_partition_count; i++) {
            if ((int)ahci_partitions[i].ahci_port != disk_id)
                continue;
            if (entry++ == index) {
                *out_name = ahci_partitions[i].name;
                *out_type = PROC_DIR;
                return 1;
            }
        }
        return 0;
    }
    if (dir_kind == 3) {
        static const char *queue_attrs[] = {"logical_block_size", "physical_block_size"};
        if (index >= sizeof(queue_attrs) / sizeof(queue_attrs[0]))
            return 0;
        *out_name = queue_attrs[index];
        *out_type = PROC_FILE;
        return 1;
    }
    if (dir_kind == 4) {
        static const char *partition_attrs[] = {"dev", "size", "ro", "removable", "partition", "start", "uevent"};
        if (index >= sizeof(partition_attrs) / sizeof(partition_attrs[0]))
            return 0;
        *out_name = partition_attrs[index];
        *out_type = PROC_FILE;
        return 1;
    }

    if (strcmp(normalized, "dev/block") == 0) {
        if (index >= (uint64_t)sysfs_block_count())
            return 0;

        static char dent_name[32];
        int major = 0, minor = 0;
        if (!sysfs_block_get_entry((int)index, &major, &minor))
            return 0;
        snprintf(dent_name, sizeof(dent_name), "%d:%d", major, minor);
        *out_name = dent_name;
        *out_type = PROC_SYMLINK;
        return 1;
    }

    if (strcmp(normalized, "bus/usb/devices") == 0) {
        if (index >= usb_device_count())
            return 0;
        usb_device_t *device = usb_get_device(index);
        if (!device)
            return 0;
        static char dent_name[32];
        snprintf(dent_name, sizeof(dent_name), "1-%u", device->root_port);
        *out_name = dent_name;
        *out_type = PROC_DIR;
        return 1;
    }

    if (strncmp(normalized, "bus/usb/devices/", 16) == 0) {
        const char *name = normalized + 16;
        if (strchr(name, '/') == NULL && sysfs_usb_index_from_name(name) >= 0) {
            if (index >= USB_ATTR_COUNT)
                return 0;
            *out_name = usb_attr_names[index];
            *out_type = PROC_FILE;
            return 1;
        }
    }

    /* bus/pci/devices -> one DIRECTORY per live PCI device */
    if (strcmp(normalized, "bus/pci/devices") == 0) {
        if (index >= (uint64_t)total_devices)
            return 0;

        static char dent_name[32];
        sysfs_pci_name((int)index, dent_name, sizeof(dent_name));
        *out_name = dent_name;
        *out_type = PROC_DIR;
        return 1;
    }

    /* bus/pci/devices/<addr> -> the attribute files */
    int dev, attr;
    if (sysfs_pci_parse(normalized, &dev, &attr) == 1 && attr < 0) {
        if (index >= PCI_ATTR_COUNT)
            return 0;

        *out_name = pci_attr_names[index];
        *out_type = PROC_FILE;
        return 1;
    }

    return ns_getdent(sys_static_files, sys_static_file_count, normalized, index, out_name, out_type);
}

int sysfs_ls(const char *path) {
    uint64_t i = 0;
    const char *name;
    procfs_type_t type;
    while (sysfs_getdent(path, i++, &name, &type)) {
        printfnoln(type == PROC_DIR ? blue_color "%s/ " reset_color : green_color "%s " reset_color, name);
    }
    return 0;
}