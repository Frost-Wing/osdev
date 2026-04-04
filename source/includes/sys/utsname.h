/**
 * @file utsname.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef SYS_UTSNAME_H
#define SYS_UTSNAME_H

#include <filesystems/vfs.h>
#include <graphics.h>
#include <heap.h>
#include <stdint.h>
#include <memory.h>
#include <versions.h>

#ifndef LINUX_EINVAL
#define LINUX_EINVAL 22
#endif

#ifndef LINUX_ENOMEM
#define LINUX_ENOMEM 12
#endif 

/**
 * @brief System identification information.
 *
 * Contains details about the system such as OS name, node name,
 * release version, and hardware architecture.
 * Filled by the uname syscall.
 */
typedef struct {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} linux_utsname_t;


/**
 * @brief Retrieves system identification information.
 *
 * Fills the provided linux_utsname_t structure with system metadata
 * such as OS name, version, and architecture.
 *
 * @param uts Pointer to a linux_utsname_t structure to populate.
 *
 * @return 0 on success.
 * @retval -LINUX_EINVAL If @p uts is NULL.
 *
 * @note All strings are guaranteed to be null-terminated.
 */
static int64 sys_uname(linux_utsname_t* uts) {
    if (!uts)
        return -LINUX_EINVAL;

    // Optional: zeroing is not strictly needed if we overwrite everything
    memset(uts, 0, sizeof(*uts));

    // Use a small macro/helper to reduce repetition
    #define SET_FIELD(field, value) \
        snprintf((field), sizeof(field), "%s", (value))

    SET_FIELD(uts->sysname,    "FrostWing");
    SET_FIELD(uts->nodename,   "localhost");

    vfs_file_t hostf = {0};
    if (vfs_open("/etc/hostname", VFS_RDONLY, &hostf) == 0) {
        char hostbuf[sizeof(uts->nodename)];
        uint32_t total = 0;

        while (total < (sizeof(hostbuf) - 1)) {
            int r = vfs_read(&hostf, (uint8_t*)hostbuf + total, sizeof(hostbuf) - 1 - total);
            if (r <= 0)
                break;
            total += (uint32_t)r;
        }

        vfs_close(&hostf);

        hostbuf[total] = '\0';
        while (total > 0 &&
               (hostbuf[total - 1] == '\n' || hostbuf[total - 1] == '\r')) {
            hostbuf[--total] = '\0';
        }

        if (total > 0)
            SET_FIELD(uts->nodename, hostbuf);
    } else {
        printf("open failed!");
    }
    
    SET_FIELD(uts->release,    "v0.1-prebuild-construct");
    SET_FIELD(uts->version,    CONCAT("fw-kernel (Build Stamp:", date, ")"));
    SET_FIELD(uts->machine,    "x86_64");
    SET_FIELD(uts->domainname, "localdomain");

    #undef SET_FIELD

    return 0;
}


#endif
