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

#include <commands/commands.h>
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

    // sample code begin - open doesnt work since multiple open could not be done??
    char* argv[2] = {"cat", "/etc/hostname"};
    cmd_cat(2, &argv[0]);
    // sample code end 
    
    SET_FIELD(uts->release,    "v0.1-prebuild-construct");
    SET_FIELD(uts->version,    CONCAT("fw-kernel (Build Stamp:", date, ")"));
    SET_FIELD(uts->machine,    "x86_64");
    SET_FIELD(uts->domainname, "localdomain");

    #undef SET_FIELD

    return 0;
}


#endif