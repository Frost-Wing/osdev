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
#include <filesystems/layers/proc.h>
#include <basics.h>
#include <strings.h>
#include <heap.h>
#include <memory.h>

#define PROCFS_MAX_FILES 32

static procfs_entry_t* proc_files[PROCFS_MAX_FILES];
static int proc_file_count = 0;

/* CODE FOR PROC FILES */

static int proc_stat_read(
    vfs_file_t* file,
    uint8_t* buf,
    uint32_t size,
    void* priv
) {
    (void)priv;

    char tmp[128];
    int len = snprintf(tmp, sizeof(tmp),
        "cpu  0 0 0 0\n"
    );

    if (file->pos >= (uint32_t)len)
        return 0;

    uint32_t rem = len - file->pos;
    if (rem > size) rem = size;

    memcpy(buf, tmp + file->pos, rem);
    file->pos += rem;
    return rem;
}

static procfs_entry_t proc_stat = {
    .name  = "stat",
    .read  = proc_stat_read,
    .write = NULL,
    .priv  = NULL
};

static int proc_heap_read(
    vfs_file_t* file,
    uint8_t* buf,
    uint32_t size,
    void* priv
) {
    (void)priv;

    char tmp[128];
    int len = snprintf(tmp, sizeof(tmp),
        "HeapTotal: %u bytes\nHeapUsed: %u bytes\nHeapFree: %u bytes\nAllocCount: %u",
        (heap_end - heap_begin),
        (memory_used),
        (heap_end - last_alloc),
        alloc_count
    );

    if (file->pos >= (uint32_t)len)
        return 0;

    uint32_t rem = len - file->pos;
    if (rem > size) rem = size;

    memcpy(buf, tmp + file->pos, rem);
    file->pos += rem;
    return rem;
}

static procfs_entry_t proc_heap = {
    .name  = "heap",
    .read  = proc_heap_read,
    .write = NULL,
    .priv  = NULL
};

extern struct memory_context* limine_memory_ctx;

static int proc_meminfo_read(vfs_file_t* file, uint8_t* buf, uint32_t size, void* priv) {
    (void)priv;

    char tmp[512];
    int len = 0;

    // Convert bytes to KB
    uint64_t kb_total       = limine_memory_ctx->total / 1024;
    uint64_t kb_free        = limine_memory_ctx->usable / 1024;
    uint64_t kb_reserved    = limine_memory_ctx->reserved / 1024;
    uint64_t kb_acpi_reclaim= limine_memory_ctx->acpi_reclaimable / 1024;
    uint64_t kb_acpi_nvs    = limine_memory_ctx->acpi_nvs / 1024;
    uint64_t kb_bad         = limine_memory_ctx->bad / 1024;
    uint64_t kb_boot        = limine_memory_ctx->bootloader_reclaimable / 1024;
    uint64_t kb_kernel      = limine_memory_ctx->kernel_modules / 1024;
    uint64_t kb_fb          = limine_memory_ctx->framebuffer / 1024;
    uint64_t kb_unknown     = limine_memory_ctx->unknown / 1024;

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
        kb_acpi_nvs, kb_bad, kb_boot, kb_kernel, kb_fb, kb_unknown
    );

    // Handle file offset for multiple reads
    if (file->pos >= (uint32_t)len) return 0;

    uint32_t rem = len - file->pos;
    if (rem > size) rem = size;

    memcpy(buf, tmp + file->pos, rem);
    file->pos += rem;
    return rem;
}

static procfs_entry_t proc_meminfo = {
    .name  = "meminfo",
    .read  = proc_meminfo_read,
    .write = NULL,
    .priv  = NULL
};

/* END */

void procfs_init(void) {
    proc_file_count = 0;
    memset(proc_files, 0, sizeof(proc_files));
    procfs_register(&proc_stat);
    procfs_register(&proc_heap);
    procfs_register(&proc_meminfo);
}

/* Register a virtual proc file */
int procfs_register(procfs_entry_t* entry) {
    if (proc_file_count >= PROCFS_MAX_FILES)
        return -1;

    proc_files[proc_file_count++] = entry;
    return 0;
}

/* Find proc entry by rel_path */
static procfs_entry_t* procfs_find(const char* name) {
    for (int i = 0; i < proc_file_count; i++) {
        if (strcmp(proc_files[i]->name, name) == 0)
            return proc_files[i];
    }
    return NULL;
}

int procfs_open(vfs_file_t* file) {
    if (!file || !file->rel_path)
        return -1;

    procfs_entry_t* e = procfs_find(file->rel_path);
    if (!e)
        return -1; // file doesn't exist

    file->pos = 0;
    return 0;
}

int procfs_read(vfs_file_t* file, uint8_t* buf, uint32_t size) {
    if (!file || !file->rel_path || !buf)
        return -1;

    procfs_entry_t* e = procfs_find(file->rel_path);
    if (!e) return -1;        // file not found
    if (!e->read) return -1;  // read not supported

    return e->read(file, buf, size, e->priv);
}

int procfs_write(vfs_file_t* file, const uint8_t* buf, uint32_t size) {
    if (!file || !file->rel_path || !buf)
        return -1;

    procfs_entry_t* e = procfs_find(file->rel_path);
    if (!e) return -1;
    if (!e->write) return -1;

    return e->write(file, buf, size, e->priv);
}


void procfs_close(vfs_file_t* file) {
    (void)file;
}

int procfs_ls(void) {
    for (int i = 0; i < proc_file_count; i++) {
        printfnoln(blue_color "%s " reset_color, proc_files[i]->name);
    }
    return 0;
}