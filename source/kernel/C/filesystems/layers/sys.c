
/* ============================================================================
 * SYSFS COMPATIBILITY LAYER
 *
 * Toybox probes /sys/bus/pci/devices when discovering PCI devices. FrostWing
 * already exposes PCI details through procfs, so sysfs keeps a tiny read-only
 * mirror at the Linux-compatible PCI path.
 *
 * The static part of that tree (bus/, bus/pci/, bus/pci/devices/) is just
 * regular entries served by the same shared namespace engine procfs uses.
 * Only the *leaves* under bus/pci/devices/<addr> are special: there's one
 * per live PCI device, generated on demand rather than registered up front,
 * so those two spots (sysfs_find_dynamic / the dynamic branch in
 * sysfs_getdent) are the only sysfs-specific code left in this file.
 * ==========================================================================*/
#include <basics.h>
#include <filesystems/layers/namespace.h>
#include <filesystems/layers/proc.h>
#include <pci.h>
#include <memory.h>

#define SYSFS_STATIC_FILES 3

typedef struct {
    int index;
} sysfs_pci_priv_t;

static procfs_entry_t *sys_static_files[SYSFS_STATIC_FILES];
static int sys_static_file_count = 0;
static procfs_entry_t sys_pci_entry;
static sysfs_pci_priv_t sys_pci_priv;

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

extern int proc_pci_device_read(
    vfs_file_t *file,
    uint8_t *buf,
    uint32_t size,
    void *priv);

void sysfs_init(void) {
    sys_static_file_count = 0;
    memset(sys_static_files, 0, sizeof(sys_static_files));
    sys_static_files[sys_static_file_count++] = &sys_bus;
    sys_static_files[sys_static_file_count++] = &sys_bus_pci;
    sys_static_files[sys_static_file_count++] = &sys_bus_pci_devices_dir;
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

/* The one bit of sysfs that the generic engine genuinely can't do: a
 * per-device leaf that doesn't exist as a registered entry until you ask
 * for it by its (dynamically formatted) PCI address. */
static procfs_entry_t *sysfs_find_dynamic(const char *normalized) {
    static const char prefix[] = "bus/pci/devices/";
    size_t prefix_len = sizeof(prefix) - 1;

    if (strncmp(normalized, prefix, prefix_len) != 0)
        return NULL;

    int index = sysfs_pci_index_from_name(normalized + prefix_len);
    if (index < 0)
        return NULL;

    sys_pci_priv.index = index;
    sys_pci_entry.name = normalized;
    sys_pci_entry.type = PROC_FILE;
    sys_pci_entry.read = proc_pci_device_read;
    sys_pci_entry.write = NULL;
    sys_pci_entry.priv = &sys_pci_priv;
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

    /* One dynamic entry per live PCI device - can't be a static array. */
    if (strcmp(normalized, "bus/pci/devices") == 0) {
        if (index >= (uint64_t)total_devices)
            return 0;

        static char dent_name[32];
        sysfs_pci_name((int)index, dent_name, sizeof(dent_name));
        *out_name = dent_name;
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