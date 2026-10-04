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
#include <ahci.h>
#include <basics.h>
#include <filesystems/layers/namespace.h>
#include <filesystems/layers/proc.h>
#include <heap.h>
#include <memory.h>
#include <pci.h>
#include <ringbuffer.h>
#include <strings.h>
#include <multitasking.h>
#include <net/net.h>
#include <xhci.h>

#define PROCFS_MAX_FILES (MAX_PCI_DEVICES + 20)

#ifndef PIT_HZ
#define PIT_HZ 100
#endif
#define PROC_USER_HZ 100    /* what Linux userspace assumes (sysconf(_SC_CLK_TCK)) */

static procfs_entry_t *proc_files[PROCFS_MAX_FILES];
static int proc_file_count = 0;

/* ============================== PROC FILES ============================= */

static const char *proc_fs_type_name(partition_fs_type_t type) {
    switch (type) {
        case FS_ISO9660:
            return "iso9660";
        case FS_FAT16:
            return "fat16";
        case FS_FAT32:
            return "fat32";
        case FS_EXT2:
            return "ext2";
        case FS_PROC:
            return "proc";
        case FS_SYS:
            return "sys";
        case FS_DEV:
            return "dev";
        default:
            return "unknown";
    }
}

static uint32_t proc_ticks_to_clk(uint64_t ticks) {
    return (uint32_t)((ticks * PROC_USER_HZ) / PIT_HZ);
}

extern uint64_t boot_unix_time;

static int proc_stat_read(vfs_file_t *file, uint8_t *buf, uint32_t size, void *priv) {
    (void)priv;

    char tmp[512];
    uint32_t idle = proc_ticks_to_clk(multitasking_now_ticks());

    int len = snprintf(tmp, sizeof(tmp),
        "cpu  0 0 0 %u 0 0 0 0 0 0\n"
        "cpu0 0 0 0 %u 0 0 0 0 0 0\n"
        "intr 0\n"
        "ctxt 0\n"
        "btime %u\n"
        "processes %u\n"
        "procs_running %u\n"
        "procs_blocked 0\n",
        idle, idle,
        boot_unix_time,
        multitasking_last_pid(),
        multitasking_count_running());

    if (len < 0) len = 0;
    if ((size_t)len >= sizeof(tmp)) len = sizeof(tmp) - 1;
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

static int proc_mounts_read(
    vfs_file_t *file,
    uint8_t *buf,
    uint32_t size,
    void *priv) {
    (void)priv;

    char tmp[1024];
    int len = 0;

    for (int i = 0; i < mounted_partition_count; i++) {
        mount_entry_t *m = &mounted_partitions[i];

        const char *dev = (m->part_name && *m->part_name) ? m->part_name : "none";
        const char *mnt = (m->mount_point && *m->mount_point) ? m->mount_point : "/";
        const char *fst = proc_fs_type_name(m->type);

        int n = snprintf(tmp + len, sizeof(tmp) - (size_t)len,
            "%s %s %s rw 0 0\n",
            dev, mnt, fst);

        if (n < 0 || len + n >= (int)sizeof(tmp))
            break; // ran out of scratch buffer, truncate cleanly

        len += n;
    }

    return ns_reply(file, buf, size, tmp, len);
}

static procfs_entry_t proc_mounts = {
    .name = "mounts",
    .read = proc_mounts_read,
    .write = NULL,
    .priv = NULL};

static int proc_partitions_read(
    vfs_file_t *file,
    uint8_t *buf,
    uint32_t size,
    void *priv) {
    (void)priv;

    char tmp[8192];
    int len = snprintf(tmp, sizeof(tmp),
        "major minor  #blocks  name\n\n");

    for (int i = 0; i < block_device_count; i++) {
        block_device_info_t *dev = &block_devices[i];
        if (!dev->present || dev->sector_size == 0)
            continue;

        unsigned int major = dev->type == BLOCK_DEVICE_NVME ? 259U : 8U;
        unsigned int minor = (unsigned int)i * 16U;
        unsigned int partition_minor = 1;
        int n = snprintf(tmp + len, sizeof(tmp) - (size_t)len,
            "%4u %7u %9u %s\n",
            major, minor,
            (unsigned long long)((dev->total_sectors * dev->sector_size) / 1024U),
            dev->name);
        if (n < 0 || (size_t)n >= sizeof(tmp) - (size_t)len)
            break;
        len += n;

        for (int p = 0; p < general_partition_count; p++) {
            general_partition_t *part = &ahci_partitions[p];
            if (part->ahci_port != (uint64)i)
                continue;

            n = snprintf(tmp + len, sizeof(tmp) - (size_t)len,
                "%4u %7u %9u %s\n",
                major, minor + partition_minor++,
                (unsigned long long)((part->sector_count * dev->sector_size) / 1024U),
                part->name);
            if (n < 0 || (size_t)n >= sizeof(tmp) - (size_t)len)
                return ns_reply(file, buf, size, tmp, len);
            len += n;
        }
    }

    return ns_reply(file, buf, size, tmp, len);
}

static procfs_entry_t proc_partitions = {
    .name = "partitions",
    .type = PROC_FILE,
    .read = proc_partitions_read};

static int proc_filesystems_read(vfs_file_t *file, uint8_t *buf,
    uint32_t size, void *priv) {
    (void)priv;
    const char *contents =
        "nodev\tproc\n"
        "nodev\tsysfs\n"
        "nodev\tdevfs\n"
        "\tvfat\n"
        "\text2\n"
        "\tiso9660\n";
    return ns_reply(file, buf, size, contents, (int)strlen(contents));
}

static procfs_entry_t proc_filesystems = {
    .name = "filesystems",
    .type = PROC_FILE,
    .read = proc_filesystems_read};

static int proc_modules_read(vfs_file_t *file, uint8_t *buf,
    uint32_t size, void *priv) {
    (void)priv;
    return ns_reply(file, buf, size, "", 0);
}

static procfs_entry_t proc_modules = {
    .name = "modules",
    .type = PROC_FILE,
    .read = proc_modules_read};

static int proc_net_dev_read(vfs_file_t *file, uint8_t *buf,
    uint32_t size, void *priv) {
    (void)priv;
    char contents[512];
    int len = snprintf(contents, sizeof(contents),
        "Inter-|   Receive                                                |  Transmit\n"
        " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
        "    lo: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
        "  eth0: %u %u %u 0 0 0 0 0 %u %u %u 0 0 0 0 0\n"
        "rx_resets=%u rx_overflow=%u rx_bad_header=%u rx_queue_drops=%u ip_bad_checksum=%u udp_bad_checksum=%u tcp_bad_checksum=%u tcp_rst_rx=%u tcp_rst_ignored=%u tcp_ooo_drops=%u tcp_rx_full_drops=%u tcp_retransmits=%u udp_no_socket_drops=%u\n",
        (unsigned long long)netif_stats.rx_bytes,
        (unsigned long long)netif_stats.rx_packets,
        (unsigned long long)netif_stats.rx_errors,
        (unsigned long long)netif_stats.tx_bytes,
        (unsigned long long)netif_stats.tx_packets,
        (unsigned long long)netif_stats.tx_errors,
        (unsigned long long)netif_stats.rx_resets,
        (unsigned long long)netif_stats.rx_overflow,
        (unsigned long long)netif_stats.rx_bad_header,
        (unsigned long long)netif_stats.rx_queue_drops,
        (unsigned long long)netif_stats.ip_bad_checksum,
        (unsigned long long)netif_stats.udp_bad_checksum,
        (unsigned long long)netif_stats.tcp_bad_checksum,
        (unsigned long long)netif_stats.tcp_rst_rx,
        (unsigned long long)netif_stats.tcp_rst_ignored,
        (unsigned long long)netif_stats.tcp_ooo_drops,
        (unsigned long long)netif_stats.tcp_rx_full_drops,
        (unsigned long long)netif_stats.tcp_retransmits,
        (unsigned long long)netif_stats.udp_no_socket_drops);
    if (len < 0)
        return -1;
    if ((size_t)len >= sizeof(contents))
        len = sizeof(contents) - 1;
    return ns_reply(file, buf, size, contents, len);
}

static procfs_entry_t proc_net_dev = {
    .name = "net/dev",
    .type = PROC_FILE,
    .read = proc_net_dev_read};

static int proc_usb_devices_read(vfs_file_t *file, uint8_t *buf,
    uint32_t size, void *priv) {
    (void)priv;
    char tmp[8192];
    int len = 0;
    for (size_t i = 0; i < usb_device_count(); ++i) {
        usb_device_t *device = usb_get_device(i);
        if (!device)
            continue;
        int n = snprintf(tmp + len, sizeof(tmp) - (size_t)len,
            "T: Bus=001 Dev#=%03u Spd=%u\n"
            "P: Vendor=%04x ProdID=%04x Rev=%x.%02x\n"
            "S: Manufacturer=%u Product=%u SerialNumber=%u\n\n",
            device->slot_id, device->speed,
            device->descriptor.vendor_id, device->descriptor.product_id,
            device->descriptor.device_version >> 8,
            device->descriptor.device_version & 0xFFU,
            device->descriptor.manufacturer_index,
            device->descriptor.product_index,
            device->descriptor.serial_index);
        if (n < 0 || (size_t)n >= sizeof(tmp) - (size_t)len)
            break;
        len += n;
    }
    return ns_reply(file, buf, size, tmp, len);
}

static procfs_entry_t proc_usb_devices = {
    .name = "bus/usb/devices",
    .type = PROC_FILE,
    .read = proc_usb_devices_read};

extern struct memory_context *limine_memory_ctx;

static int proc_meminfo_read(vfs_file_t *file, uint8_t *buf, uint32_t size, void *priv) {
    (void)priv;

    char tmp[768];

    uint32_t kb_total = (uint32_t)(limine_memory_ctx->total / 1024);
    uint32_t kb_free = (uint32_t)(limine_memory_ctx->usable / 1024);
    uint32_t kb_reserved = (uint32_t)(limine_memory_ctx->reserved / 1024);
    uint32_t kb_acpi_reclaim = (uint32_t)(limine_memory_ctx->acpi_reclaimable / 1024);
    uint32_t kb_acpi_nvs = (uint32_t)(limine_memory_ctx->acpi_nvs / 1024);
    uint32_t kb_bad = (uint32_t)(limine_memory_ctx->bad / 1024);
    uint32_t kb_boot = (uint32_t)(limine_memory_ctx->bootloader_reclaimable / 1024);
    uint32_t kb_kernel = (uint32_t)(limine_memory_ctx->kernel_modules / 1024);
    uint32_t kb_fb = (uint32_t)(limine_memory_ctx->framebuffer / 1024);
    uint32_t kb_unknown = (uint32_t)(limine_memory_ctx->unknown / 1024);

    int len = snprintf(tmp, sizeof(tmp),
        "MemTotal:       %u kB\n"
        "MemFree:        %u kB\n"
        "MemAvailable:   %u kB\n"
        "Buffers:        0 kB\n"
        "Cached:         0 kB\n"
        "SwapCached:     0 kB\n"
        "SwapTotal:      0 kB\n"
        "SwapFree:       0 kB\n"
        "Shmem:          0 kB\n"
        "Slab:           0 kB\n"
        "MemReserved:    %u kB\n"
        "ACPIReclaim:    %u kB\n"
        "ACPINVS:        %u kB\n"
        "BadMem:         %u kB\n"
        "Bootloader:     %u kB\n"
        "KernelModules:  %u kB\n"
        "Framebuffer:    %u kB\n"
        "Unknown:        %u kB\n",
        kb_total, kb_free, kb_free,
        kb_reserved, kb_acpi_reclaim, kb_acpi_nvs, kb_bad,
        kb_boot, kb_kernel, kb_fb, kb_unknown);

    if (len < 0) len = 0;
    if ((size_t)len >= sizeof(tmp)) len = sizeof(tmp) - 1;
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

/* ============================ /proc/uptime, loadavg ==================== */

static int proc_uptime_read(vfs_file_t *file, uint8_t *buf, uint32_t size, void *priv) {
    (void)priv;

    uint64_t ticks = multitasking_now_ticks();
    uint32_t secs = (uint32_t)(ticks / PIT_HZ);
    uint32_t cs = (uint32_t)(((ticks % PIT_HZ) * 100) / PIT_HZ);

    char tmp[64];
    /* "%02u" spelled out by hand so we don't depend on snprintf flag support */
    int len = snprintf(tmp, sizeof(tmp), "%u.%u%u %u.%u%u\n",
                       secs, cs / 10, cs % 10, secs, cs / 10, cs % 10);
    if (len < 0) len = 0;
    if ((size_t)len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    return ns_reply(file, buf, size, tmp, len);
}

static procfs_entry_t proc_uptime = {
    .name = "uptime", .type = PROC_FILE, .read = proc_uptime_read};

static int proc_loadavg_read(vfs_file_t *file, uint8_t *buf, uint32_t size, void *priv) {
    (void)priv;

    char tmp[64];
    int len = snprintf(tmp, sizeof(tmp), "0.00 0.00 0.00 %u/%u %u\n",
                       multitasking_count_running(),
                       multitasking_count_tasks(),
                       multitasking_last_pid());
    if (len < 0) len = 0;
    if ((size_t)len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    return ns_reply(file, buf, size, tmp, len);
}

static procfs_entry_t proc_loadavg = {
    .name = "loadavg", .type = PROC_FILE, .read = proc_loadavg_read};

static int proc_pid_max_read(vfs_file_t *file, uint8_t *buf, uint32_t size, void *priv) {
    (void)priv;
    const char *s = "32768\n";
    return ns_reply(file, buf, size, s, (int)strlen(s));
}

static procfs_entry_t proc_pid_max = {
    .name = "sys/kernel/pid_max", .type = PROC_FILE, .read = proc_pid_max_read};

/* ============================ /proc/<pid>/... ========================== */

#define PROC_PID_BUF 1024

typedef int (*proc_pid_gen_fn)(const task_info_t *ti, char *buf, size_t sz);

/* Linux truncates comm to 15 chars and it is the basename, not a path. */
static void proc_comm(const task_info_t *ti, char *out /* >= 16 */) {
    const char *base = ti->name;
    for (const char *p = ti->name; *p; p++) {
        if (*p == '/' && p[1])
            base = p + 1;
    }
    size_t n = 0;
    while (base[n] && n < 15) {
        out[n] = base[n];
        n++;
    }
    out[n] = '\0';
}

static const char *proc_state_letter(task_state_t s) {
    switch (s) {
        case TASK_STATE_READY:    return "R";
        case TASK_STATE_RUNNING:  return "R";
        case TASK_STATE_SLEEPING: return "S";
        case TASK_STATE_EXITED:   return "Z"; /* zombie until the parent reaps it */
        default:                  return "R";
    }
}

static const char *proc_state_long(task_state_t s) {
    switch (s) {
        case TASK_STATE_SLEEPING: return "S (sleeping)";
        case TASK_STATE_EXITED:   return "Z (zombie)";
        default:                  return "R (running)";
    }
}

static int proc_pid_cmdline(const task_info_t *ti, char *buf, size_t sz) {
    int n = multitasking_get_cmdline(ti->pid, buf, sz - 1);
    return n < 0 ? 0 : n;
}

static int proc_pid_environ(const task_info_t *ti, char *buf, size_t sz) {
    (void)ti; (void)buf; (void)sz;
    return 0; /* empty, like a process with no environment */
}

static int proc_pid_comm(const task_info_t *ti, char *buf, size_t sz) {
    char comm[16];
    proc_comm(ti, comm);
    return snprintf(buf, sz, "%s\n", comm);
}

static int proc_pid_statm(const task_info_t *ti, char *buf, size_t sz) {
    (void)ti;
    return snprintf(buf, sz, "0 0 0 0 0 0 0\n");
}

static int proc_pid_stat(const task_info_t *ti, char *buf, size_t sz) {
    char comm[16];
    proc_comm(ti, comm);

    bool is_user = ti->type == TASK_TYPE_USERLAND;
    /* tty1 = char device 4:1, encoded the way Linux does: (major << 8) | minor */
    int tty_nr = is_user ? (int)((4 << 8) | (ti->tty_index + 1)) : 0;
    int tpgid = is_user ? (int)ti->pid : -1;
    unsigned flags = is_user ? 0u : 0x00200000u; /* PF_KTHREAD for kernel tasks */
    int exit_status = ti->state == TASK_STATE_EXITED ? ((ti->exit_code & 0xff) << 8) : 0;

    /* All 52 fields of proc(5) /proc/<pid>/stat, in order. */
    return snprintf(buf, sz,
        "%u (%s) %s %u %u %u %d %d %u "      /* 1-9   pid comm state ppid pgrp session tty tpgid flags */
        "0 0 0 0 "                           /* 10-13 minflt cminflt majflt cmajflt */
        "%u 0 0 0 "                          /* 14-17 utime stime cutime cstime */
        "20 0 1 0 "                          /* 18-21 priority nice num_threads itrealvalue */
        "%u "                                /* 22    starttime */
        "0 0 18446744073709551615 "          /* 23-25 vsize rss rsslim */
        "0 0 0 0 0 0 0 0 0 0 0 0 "           /* 26-37 code/stack/eip/signals/wchan/nswap/cnswap */
        "17 0 0 0 0 0 0 "                    /* 38-44 exit_signal processor rt_prio policy blkio guest cguest */
        "0 0 0 0 0 0 "                       /* 45-50 start_data end_data start_brk arg/env ranges */
        "0 "                                 /* 51    env_end */
        "%d\n",                              /* 52    exit_code */
        ti->pid, comm, proc_state_letter(ti->state),
        ti->parent_pid, ti->pid, ti->pid, tty_nr, tpgid, flags,
        proc_ticks_to_clk(ti->runtime_ticks),
        proc_ticks_to_clk(ti->created_at_tick),
        exit_status);
}

static int proc_pid_status(const task_info_t *ti, char *buf, size_t sz) {
    char comm[16];
    proc_comm(ti, comm);

    return snprintf(buf, sz,
        "Name:\t%s\n"
        "Umask:\t0022\n"
        "State:\t%s\n"
        "Tgid:\t%u\n"
        "Ngid:\t0\n"
        "Pid:\t%u\n"
        "PPid:\t%u\n"
        "TracerPid:\t0\n"
        "Uid:\t0\t0\t0\t0\n"
        "Gid:\t0\t0\t0\t0\n"
        "FDSize:\t64\n"
        "Groups:\t\n"
        "VmSize:\t0 kB\n"
        "VmRSS:\t0 kB\n"
        "Threads:\t1\n"
        "SigQ:\t0/0\n"
        "SigPnd:\t0000000000000000\n"
        "ShdPnd:\t0000000000000000\n"
        "SigBlk:\t0000000000000000\n"
        "SigIgn:\t0000000000000000\n"
        "SigCgt:\t0000000000000000\n"
        "Cpus_allowed_list:\t0\n"
        "voluntary_ctxt_switches:\t0\n"
        "nonvoluntary_ctxt_switches:\t0\n",
        comm, proc_state_long(ti->state), ti->pid, ti->pid, ti->parent_pid);
}

static const struct {
    const char *name;
    proc_pid_gen_fn gen;
} proc_pid_files[] = {
    {"cmdline", proc_pid_cmdline},
    {"comm", proc_pid_comm},
    {"environ", proc_pid_environ},
    {"stat", proc_pid_stat},
    {"statm", proc_pid_statm},
    {"status", proc_pid_status},
};
#define PROC_PID_FILE_COUNT ((int)(sizeof(proc_pid_files) / sizeof(proc_pid_files[0])))

/* Classify a path:
 *   -1  not a pid path at all (fall through to the static namespace)
 *    0  looks like a pid path but is invalid / the task doesn't exist
 *    1  valid; *file == -1 for the "<pid>" directory itself, else an index
 *       into proc_pid_files[] */
static int proc_pid_resolve(const char *path, uint32_t *pid_out, int *file_out) {
    char norm[256];
    ns_normalize_path(path, norm, sizeof(norm));

    const char *p = norm;
    uint32_t pid = 0;

    if (strncmp(p, "self", 4) == 0 && (p[4] == '\0' || p[4] == '/')) {
        pid = multitasking_current_pid();
        p += 4;
    } else if (*p >= '0' && *p <= '9') {
        while (*p >= '0' && *p <= '9') {
            if (pid > 0x0FFFFFFF)
                return 0;
            pid = pid * 10 + (uint32_t)(*p - '0');
            p++;
        }
    } else {
        return -1;
    }

    if (*p == '/')
        p++;
    else if (*p != '\0')
        return 0;

    task_info_t ti;
    if (!multitasking_get_task(pid, &ti))
        return 0;

    *pid_out = pid;

    if (*p == '\0') {
        *file_out = -1;
        return 1;
    }

    for (int i = 0; i < PROC_PID_FILE_COUNT; i++) {
        if (strcmp(p, proc_pid_files[i].name) == 0) {
            *file_out = i;
            return 1;
        }
    }
    return 0;
}

static int proc_pid_read(vfs_file_t *file, uint8_t *buf, uint32_t size, uint32_t pid, int idx) {
    task_info_t ti;
    if (!multitasking_get_task(pid, &ti))
        return -1;

    char tmp[PROC_PID_BUF];
    int len = proc_pid_files[idx].gen(&ti, tmp, sizeof(tmp));
    if (len < 0) len = 0;
    if ((size_t)len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    return ns_reply(file, buf, size, tmp, len);
}

typedef struct {
    uint64_t want;
    uint64_t seen;
    uint32_t pid;
    bool found;
} proc_pid_cursor_t;

static bool proc_pid_cursor_cb(const task_info_t *info, void *ctx) {
    proc_pid_cursor_t *c = ctx;
    if (c->seen++ == c->want) {
        c->pid = info->pid;
        c->found = true;
        return false; /* stop iterating */
    }
    return true;
}

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
    procfs_register(&proc_mounts);
    procfs_register(&proc_partitions);
    procfs_register(&proc_filesystems);
    procfs_register(&proc_modules);
    procfs_register(&proc_net_dev);
    procfs_register(&proc_usb_devices);
    procfs_register(&proc_uptime);
    procfs_register(&proc_loadavg);
    procfs_register(&proc_pid_max);
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

    uint32_t pid;
    int idx;
    int r = proc_pid_resolve(file->rel_path, &pid, &idx);
    if (r >= 0) {
        if (r != 1)
            return -1; // no such process / file
        file->pos = 0;
        return 0;
    }

    if (!ns_find(proc_files, proc_file_count, file->rel_path))
        return -1; // file doesn't exist

    file->pos = 0;
    return 0;
}

int procfs_read(vfs_file_t *file, uint8_t *buf, uint32_t size) {
    if (!file || !file->rel_path || !buf)
        return -1;

    uint32_t pid;
    int idx;
    int r = proc_pid_resolve(file->rel_path, &pid, &idx);
    if (r >= 0) {
        if (r != 1 || idx < 0)
            return -1; // missing, or it's a directory
        return proc_pid_read(file, buf, size, pid, idx);
    }

    procfs_entry_t *e = ns_find(proc_files, proc_file_count, file->rel_path);
    if (!e || !e->read)
        return -1;

    return e->read(file, buf, size, e->priv);
}

int procfs_write(vfs_file_t *file, const uint8_t *buf, uint32_t size) {
    if (!file || !file->rel_path || !buf)
        return -1;

    uint32_t pid;
    int idx;
    if (proc_pid_resolve(file->rel_path, &pid, &idx) >= 0)
        return -1; // per-process files are read-only

    procfs_entry_t *e = ns_find(proc_files, proc_file_count, file->rel_path);
    if (!e || !e->write)
        return -1;

    return e->write(file, buf, size, e->priv);
}

int procfs_getdent(const char *path, uint64_t index, const char **out_name, procfs_type_t *out_type) {
    if (!path || !out_name || !out_type)
        return 0;

    /* /proc/<pid> (or /proc/self): list the per-process files */
    uint32_t pid;
    int idx;
    int r = proc_pid_resolve(path, &pid, &idx);
    if (r >= 0) {
        if (r != 1 || idx >= 0 || index >= (uint64_t)PROC_PID_FILE_COUNT)
            return 0;
        *out_name = proc_pid_files[index].name;
        *out_type = PROC_FILE;
        return 1;
    }

    /* Static entries first */
    if (ns_getdent(proc_files, proc_file_count, path, index, out_name, out_type))
        return 1;

    /* The root additionally lists "self", then one directory per live task */
    char norm[256];
    ns_normalize_path(path, norm, sizeof(norm));
    if (norm[0] != '\0')
        return 0;

    uint64_t static_count = 0;
    const char *dummy_name;
    procfs_type_t dummy_type;
    while (ns_getdent(proc_files, proc_file_count, path, static_count, &dummy_name, &dummy_type))
        static_count++;

    if (index < static_count)
        return 0;

    if (index == static_count) {
        *out_name = "self";
        *out_type = PROC_DIR; /* Linux makes this a symlink; no symlink type here yet */
        return 1;
    }

    proc_pid_cursor_t cur = { .want = index - static_count - 1, .seen = 0, .pid = 0, .found = false };
    multitasking_for_each_task(proc_pid_cursor_cb, &cur);
    if (!cur.found)
        return 0;

    static char pid_name[16];
    snprintf(pid_name, sizeof(pid_name), "%u", cur.pid);
    *out_name = pid_name;
    *out_type = PROC_DIR;
    return 1;
}

int procfs_ls(const char *path) {
    uint64_t i = 0;
    const char *name;
    procfs_type_t type;

    while (procfs_getdent(path, i++, &name, &type)) {
        printfnoln(
            type == PROC_DIR ? blue_color "%s/ " reset_color : green_color "%s " reset_color,
            name);
    }
    return 0;
}

int procfs_path_is_dir(const char *path) {
    if (!path)
        return -1;

    uint32_t pid;
    int idx;
    int r = proc_pid_resolve(path, &pid, &idx);
    if (r >= 0) {
        if (r != 1)
            return -1;
        return idx < 0 ? 1 : 0;
    }

    return ns_is_dir(proc_files, proc_file_count, path);
}