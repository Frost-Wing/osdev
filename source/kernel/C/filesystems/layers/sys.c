
/* ============================================================================
 * SYSFS COMPATIBILITY LAYER
 *
 * Toybox probes /sys/bus/pci/devices when discovering PCI devices. FrostWing
 * already exposes PCI details through procfs, so sysfs keeps a tiny read-only
 * mirror at the Linux-compatible PCI path.
 *
 * The static part of that tree (bus/, bus/pci/, bus/pci/devices/) is just
 * regular entries served by the same shared namespace engine procfs uses.
 * Device directories and attributes are generated on demand for PCI and USB
 * devices, so the dynamic lookup/getdent branches provide those live entries.
 * ==========================================================================*/
#include <basics.h>
#include <filesystems/layers/namespace.h>
#include <filesystems/layers/proc.h>
#include <pci.h>
#include <memory.h>
#include <xhci.h>

#define SYSFS_STATIC_FILES 5

/* Attribute files that live inside bus/pci/devices/<addr>/ */
enum { PCI_ATTR_VENDOR, PCI_ATTR_DEVICE, PCI_ATTR_CLASS, PCI_ATTR_REVISION, PCI_ATTR_UEVENT, PCI_ATTR_COUNT };
enum {
    USB_ATTR_VENDOR, USB_ATTR_PRODUCT, USB_ATTR_CLASS, USB_ATTR_SUBCLASS,
    USB_ATTR_PROTOCOL, USB_ATTR_BUSNUM, USB_ATTR_DEVNUM, USB_ATTR_SPEED,
    USB_ATTR_UEVENT, USB_ATTR_COUNT
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

void sysfs_init(void) {
    sys_static_file_count = 0;
    memset(sys_static_files, 0, sizeof(sys_static_files));
    sys_static_files[sys_static_file_count++] = &sys_bus;
    sys_static_files[sys_static_file_count++] = &sys_bus_pci;
    sys_static_files[sys_static_file_count++] = &sys_bus_pci_devices_dir;
    sys_static_files[sys_static_file_count++] = &sys_bus_usb;
    sys_static_files[sys_static_file_count++] = &sys_bus_usb_devices_dir;
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
                case 1: len = snprintf(tmp, sizeof(tmp), "12\n"); break;
                case 2: len = snprintf(tmp, sizeof(tmp), "1.5\n"); break;
                case 3: len = snprintf(tmp, sizeof(tmp), "480\n"); break;
                case 4: len = snprintf(tmp, sizeof(tmp), "5000\n"); break;
                case 5: len = snprintf(tmp, sizeof(tmp), "10000\n"); break;
                default: len = snprintf(tmp, sizeof(tmp), "unknown\n"); break;
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
    size_t alen = slash ? (size_t)(slash - rest) : strlen(rest);
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

static procfs_entry_t *sysfs_find_dynamic(const char *normalized) {
    const char *usb_prefix = "bus/usb/devices/";
    size_t prefix_len = strlen(usb_prefix);
    if (strncmp(normalized, usb_prefix, prefix_len) == 0) {
        const char *rest = normalized + prefix_len;
        const char *slash = strchr(rest, '/');
        size_t name_len = slash ? (size_t)(slash - rest) : strlen(rest);
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
    if (!file || !file->rel_path)
        return -1;

    if (!sysfs_find(file->rel_path))
        return -1;

    file->pos = 0;
    return 0;
}

int sysfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size) {
    if (!file || !file->rel_path || !buf)
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

int sysfs_getdent(const char *path, uint64_t index, const char **out_name, procfs_type_t *out_type) {
    char normalized[256];
    ns_normalize_path(path, normalized, sizeof(normalized));

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