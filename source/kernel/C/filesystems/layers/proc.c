/**
 * @file proc.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The proc folder to handle by the VFS.
 * @version 0.2
 * @date 2026-01-05
 *
 * @copyright Copyright (c) Pradosh 2026
 *
 * All the "flat array + string-prefix-implies-a-directory" traversal logic
 * that both procfs and sysfs need now lives once, in namespace.c. This file
 * is just: (1) the actual proc file bodies, and (2) thin wrappers handing
 * procfs's/sysfs's own arrays to that shared engine.
 */
#include <basics.h>
#include <filesystems/layers/namespace.h>
#include <filesystems/layers/proc.h>
#include <heap.h>
#include <memory.h>
#include <pci.h>
#include <ringbuffer.h>
#include <strings.h>

#define PROCFS_MAX_FILES (MAX_PCI_DEVICES + 8)

static procfs_entry_t *proc_files[PROCFS_MAX_FILES];
static int proc_file_count = 0;

/* ============================== PROC FILES ============================= */

static int proc_stat_read(
    vfs_file_t *file,
    uint8_t *buf,
    uint32_t size,
    void *priv) {
    (void)priv;

    char tmp[128];
    int len = snprintf(tmp, sizeof(tmp), "cpu  0 0 0 0\n");
    return ns_reply(file, buf, size, tmp, len);
}

static procfs_entry_t proc_stat = {
    .name = "stat",
    .read = proc_stat_read,
    .write = NULL,
    .priv = NULL};

static int proc_heap_read(
    vfs_file_t *file,
    uint8_t *buf,
    uint32_t size,
    void *priv) {
    (void)priv;

    if (alloc_count < 0)
        alloc_count = 0;

    char tmp[128];
    int len = snprintf(tmp, sizeof(tmp),
        "HeapTotal: %u bytes\nHeapUsed: %u bytes\nHeapFree: %u bytes\nAllocCount: %d",
        (heap_end - heap_begin),
        (memory_used),
        (heap_end - last_alloc),
        alloc_count);
    return ns_reply(file, buf, size, tmp, len);
}

static procfs_entry_t proc_heap = {
    .name = "heap",
    .read = proc_heap_read,
    .write = NULL,
    .priv = NULL};

extern struct memory_context *limine_memory_ctx;

static int proc_meminfo_read(vfs_file_t *file, uint8_t *buf, uint32_t size, void *priv) {
    (void)priv;

    char tmp[512];

    // Convert bytes to KB
    uint64_t kb_total = limine_memory_ctx->total / 1024;
    uint64_t kb_free = limine_memory_ctx->usable / 1024;
    uint64_t kb_reserved = limine_memory_ctx->reserved / 1024;
    uint64_t kb_acpi_reclaim = limine_memory_ctx->acpi_reclaimable / 1024;
    uint64_t kb_acpi_nvs = limine_memory_ctx->acpi_nvs / 1024;
    uint64_t kb_bad = limine_memory_ctx->bad / 1024;
    uint64_t kb_boot = limine_memory_ctx->bootloader_reclaimable / 1024;
    uint64_t kb_kernel = limine_memory_ctx->kernel_modules / 1024;
    uint64_t kb_fb = limine_memory_ctx->framebuffer / 1024;
    uint64_t kb_unknown = limine_memory_ctx->unknown / 1024;

    // Build meminfo string like Linux
    int len = snprintf(tmp, sizeof(tmp),
        "MemTotal:       %u kB\n"
        "MemFree:        %u kB\n"
        "MemReserved:    %u kB\n"
        "ACPI Reclaim:   %u kB\n"
        "ACPI NVS:       %u kB\n"
        "BadMem:         %u kB\n"
        "Bootloader:     %u kB\n"
        "KernelModules:  %u kB\n"
        "Framebuffer:    %u kB\n"
        "Unknown:        %u kB\n",
        kb_total, kb_free, kb_reserved, kb_acpi_reclaim,
        kb_acpi_nvs, kb_bad, kb_boot, kb_kernel, kb_fb, kb_unknown);

    return ns_reply(file, buf, size, tmp, len);
}

static procfs_entry_t proc_meminfo = {
    .name = "meminfo",
    .read = proc_meminfo_read,
    .write = NULL,
    .priv = NULL};

static procfs_entry_t proc_pci = {
    .name = "pci",
    .type = PROC_DIR,
};

extern int proc_pci_devices_read(
    vfs_file_t *file,
    uint8_t *buf,
    uint32_t size,
    void *priv);

static procfs_entry_t proc_pci_devices = {
    .name = "pci/devices",
    .type = PROC_FILE,
    .read = proc_pci_devices_read};

/* ========================= PROCFS PUBLIC API ============================
 * Every one of these is now a one- or two-line wrapper around the shared
 * engine in namespace.c - add a new proc file above via procfs_register()
 * and none of this needs to change. */

void procfs_init(void) {
    proc_file_count = 0;
    memset(proc_files, 0, sizeof(proc_files));
    procfs_register(&proc_stat);
    procfs_register(&proc_heap);
    procfs_register(&proc_meminfo);
    procfs_register(&proc_pci);
    procfs_register(&proc_pci_devices);

    proc_pci_register();
}

int procfs_register(procfs_entry_t *entry) {
    if (proc_file_count >= PROCFS_MAX_FILES)
        return -1;

    proc_files[proc_file_count++] = entry;
    return 0;
}

int procfs_open(vfs_file_t *file) {
    if (!file || !file->rel_path)
        return -1;

    if (!ns_find(proc_files, proc_file_count, file->rel_path))
        return -1; // file doesn't exist

    file->pos = 0;
    return 0;
}

int procfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size) {
    if (!file || !file->rel_path || !buf)
        return -1;

    procfs_entry_t *e = ns_find(proc_files, proc_file_count, file->rel_path);
    if (!e || !e->read)
        return -1;

    return e->read(file, buf, size, e->priv);
}

int procfs_write(vfs_file_t *file, const uint8_t *buf, uint32_t size) {
    if (!file || !file->rel_path || !buf)
        return -1;

    procfs_entry_t *e = ns_find(proc_files, proc_file_count, file->rel_path);
    if (!e || !e->write)
        return -1;

    return e->write(file, buf, size, e->priv);
}

void procfs_close(vfs_file_t *file) {
    (void)file;
}

int procfs_getdent(const char *path, uint64_t index, const char **out_name, procfs_type_t *out_type) {
    return ns_getdent(proc_files, proc_file_count, path, index, out_name, out_type);
}

int procfs_ls(const char *path) {
    return ns_ls(proc_files, proc_file_count, path);
}

int procfs_path_is_dir(const char *path) {
    return ns_is_dir(proc_files, proc_file_count, path);
}