/**
 * @file proc.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The proc folder to handle by the VFS.
 * @version 0.1
 * @date 2026-01-05
 *
 * @copyright Copyright (c) Pradosh 2026
 *
 */
#include <basics.h>
#include <filesystems/layers/proc.h>
#include <heap.h>
#include <memory.h>
#include <pci.h>
#include <ringbuffer.h>
#include <strings.h>

#define PROCFS_MAX_FILES (MAX_PCI_DEVICES + 8)

static procfs_entry_t *proc_files[PROCFS_MAX_FILES];
static int proc_file_count = 0;

/* CODE FOR PROC FILES */

static int proc_stat_read(
    vfs_file_t *file,
    uint8_t *buf,
    uint32_t size,
    void *priv) {
    (void)priv;

    char tmp[128];
    int len = snprintf(tmp, sizeof(tmp),
        "cpu  0 0 0 0\n");

    if (file->pos >= (uint32_t)len)
        return 0;

    uint32_t rem = len - file->pos;
    if (rem > size)
        rem = size;

    memcpy(buf, tmp + file->pos, rem);
    file->pos += rem;
    return rem;
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

    if (file->pos >= (uint32_t)len)
        return 0;

    uint32_t rem = len - file->pos;
    if (rem > size)
        rem = size;

    memcpy(buf, tmp + file->pos, rem);
    file->pos += rem;
    return rem;
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
    int len = 0;

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
    len += snprintf(tmp + len, sizeof(tmp) - len,
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

    // Handle file offset for multiple reads
    if (file->pos >= (uint32_t)len)
        return 0;

    uint32_t rem = len - file->pos;
    if (rem > size)
        rem = size;

    memcpy(buf, tmp + file->pos, rem);
    file->pos += rem;
    return rem;
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

/* END */

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

/* Register a virtual proc file */
int procfs_register(procfs_entry_t *entry) {
    if (proc_file_count >= PROCFS_MAX_FILES)
        return -1;

    proc_files[proc_file_count++] = entry;
    return 0;
}

/* Find proc entry by rel_path with path normalization */
static procfs_entry_t *procfs_find(const char *name) {
    if (!name || *name == '\0')
        return NULL;

    // Create a normalized version of the name without trailing slashes
    static char normalized[256];
    size_t len = strlen(name);
    
    // Copy name and strip trailing slash if present
    if (len >= sizeof(normalized))
        len = sizeof(normalized) - 1;
    
    memcpy(normalized, name, len);
    normalized[len] = '\0';
    
    // Strip trailing slash for comparison
    if (len > 0 && normalized[len - 1] == '/')
        normalized[len - 1] = '\0';
    
    for (int i = 0; i < proc_file_count; i++) {
        if (strcmp(proc_files[i]->name, normalized) == 0)
            return proc_files[i];
    }
    return NULL;
}

int procfs_open(vfs_file_t *file) {
    if (!file || !file->rel_path)
        return -1;

    procfs_entry_t *e = procfs_find(file->rel_path);
    if (!e)
        return -1; // file doesn't exist

    file->pos = 0;
    return 0;
}

int procfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size) {
    if (!file || !file->rel_path || !buf)
        return -1;

    procfs_entry_t *e = procfs_find(file->rel_path);
    if (!e)
        return -1; // file not found
    if (!e->read)
        return -1; // read not supported

    return e->read(file, buf, size, e->priv);
}

int procfs_write(vfs_file_t *file, const uint8_t *buf, uint32_t size) {
    if (!file || !file->rel_path || !buf)
        return -1;

    procfs_entry_t *e = procfs_find(file->rel_path);
    if (!e)
        return -1;
    if (!e->write)
        return -1;

    return e->write(file, buf, size, e->priv);
}

void procfs_close(vfs_file_t *file) {
    (void)file;
}

int procfs_getdent(const char *path, uint64_t index, const char **out_name, procfs_type_t *out_type) {
    if (!path || !out_name || !out_type)
        return 0;

    size_t plen = strlen(path);
    
    // Normalize path by stripping trailing slashes
    static char normalized_path[256];
    if (plen >= sizeof(normalized_path))
        plen = sizeof(normalized_path) - 1;
    
    if (plen > 0) {
        memcpy(normalized_path, path, plen);
        normalized_path[plen] = '\0';
        
        // Strip trailing slashes
        while (plen > 0 && normalized_path[plen - 1] == '/') {
            normalized_path[--plen] = '\0';
        }
    } else {
        normalized_path[0] = '\0';
    }
    
    uint64_t seen = 0;

    for (int i = 0; i < proc_file_count; i++) {
        const char *name = proc_files[i]->name;

        if (plen) {
            if (strncmp(name, normalized_path, plen) != 0)
                continue;

            name += plen;

            // After stripping path prefix, must have either '/' or end of string
            if (*name == '/')
                name++;
            else if (*name != '\0')
                continue;  // Path component doesn't match exactly
        }

        if (*name == '\0')
            continue;  // Skip entries that are empty after path stripping

        const char *slash = strchr(name, '/');
        size_t child_len = slash ? (size_t)(slash - name) : strlen(name);

        bool duplicate = false;
        for (int j = 0; j < i; j++) {
            const char *prev = proc_files[j]->name;

            if (plen) {
                if (strncmp(prev, path, plen) != 0)
                    continue;

                prev += plen;

                if (*prev == '/')
                    prev++;
                else if (*prev != '\0')
                    continue;
            }

            if (*prev == '\0')
                continue;

            const char *prev_slash = strchr(prev, '/');
            size_t prev_len = prev_slash ? (size_t)(prev_slash - prev) : strlen(prev);

            if (prev_len == child_len && strncmp(prev, name, child_len) == 0) {
                duplicate = true;
                break;
            }
        }

        if (duplicate)
            continue;

        if (seen++ == index) {
            static char dent_name[64];

            if (child_len >= sizeof(dent_name))
                child_len = sizeof(dent_name) - 1;

            memcpy(dent_name, name, child_len);
            dent_name[child_len] = '\0';

            *out_name = dent_name;
            *out_type = slash ? PROC_DIR : proc_files[i]->type;
            return 1;
        }
    }

    return 0;
}

int procfs_ls(const char *path) {
    size_t plen = strlen(path);
    
    // Normalize path by stripping trailing slashes (same as procfs_getdent)
    static char normalized_path[256];
    if (plen >= sizeof(normalized_path))
        plen = sizeof(normalized_path) - 1;
    
    if (plen > 0) {
        memcpy(normalized_path, path, plen);
        normalized_path[plen] = '\0';
        
        // Strip trailing slashes
        while (plen > 0 && normalized_path[plen - 1] == '/') {
            normalized_path[--plen] = '\0';
        }
    } else {
        normalized_path[0] = '\0';
    }

    for (int i = 0; i < proc_file_count; i++) {
        const char *name = proc_files[i]->name;

        if (plen) {
            // Check if entry starts with path prefix
            if (strncmp(name, normalized_path, plen) != 0)
                continue;

            name += plen;

            // After stripping path prefix, must have either '/' or end of string
            if (*name == '/')
                name++;
            else if (*name != '\0')
                continue;  // Path component doesn't match
        }

        // Skip if nothing left after path stripping
        if (*name == '\0')
            continue;
            
        const char *slash = strchr(name, '/');

        // Skip entries with further subdirectories (only show direct children)
        if (slash)
            continue;

        printfnoln(
            proc_files[i]->type == PROC_DIR ? blue_color "%s/ " reset_color : green_color "%s " reset_color,
            name);
    }

    return 0;
}

int procfs_path_is_dir(const char *path)
{
    if (!path)
        return -1;

    /* /proc itself */
    if (*path == '\0')
        return 1;

    procfs_entry_t *e = procfs_find(path);

    if (!e)
        return -1;

    return e->type == PROC_DIR ? 1 : 0;
}