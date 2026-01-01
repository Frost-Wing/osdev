/**
 * @file acpi-shutdown.h
 * @author Mintsuki (https://github.com/mintsuki) & Pradosh (pradoshgame@gmail.com)
 * @brief The ACPI Shutdown Header for the kernel. For full ACPI refer acpi.h.
 * @version 0.1
 * @date 2023-10-29
 * 
 * @copyright Copyright (C) 2019-2023 mintsuki and contributors.
 * 
 */
#ifndef ACPI_SHUTDOWN_H
#define ACPI_SHUTDOWN_H

#include <basics.h>

int acpi_shutdown_hack(uintptr_t direct_map_base, void   *(*find_sdt)(cstring signature, size_t index));
#endif