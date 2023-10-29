/**
 * @file acpi-shutdown.h
 * @author Mintsuki (https://github.com/mintsuki)
 * @brief The ACPI Shutdown Header for Wing kernel, this is not the full ACPI
 * @version 0.1
 * @date 2023-10-29
 * 
 * @copyright Copyright (C) 2019-2023 mintsuki and contributors.
 * 
 */
#ifndef _ACPI_SHUTDOWN_HACK_H
#define _ACPI_SHUTDOWN_HACK_H

#include <stddef.h>
#include <stdint.h>

int acpi_shutdown_hack(
        uintptr_t direct_map_base,
        void   *(*find_sdt)(const char *signature, size_t index),
        uint8_t (*inb)(uint16_t port),
        uint16_t (*inw)(uint16_t port),
        void    (*outb)(uint16_t port, uint8_t value),
        void    (*outw)(uint16_t port, uint16_t value)
    );
#endif