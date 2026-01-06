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


/* END */

void procfs_init(void) {
    proc_file_count = 0;
    memset(proc_files, 0, sizeof(proc_files));
    procfs_register(&proc_stat);
    procfs_register(&proc_heap);
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
    procfs_entry_t* e = procfs_find(file->rel_path);
    if (!e)
        return -1;

    file->pos = 0;
    return 0;
}

int procfs_read(vfs_file_t* file, uint8_t* buf, uint32_t size) {
    procfs_entry_t* e = procfs_find(file->rel_path);
    if (!e || !e->read)
        return 0;

    return e->read(file, buf, size, e->priv);
}

int procfs_write(vfs_file_t* file, const uint8_t* buf, uint32_t size) {
    procfs_entry_t* e = procfs_find(file->rel_path);
    if (!e || !e->write)
        return -1;

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