/**
 * @file fwde.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The executor header for FrostWing Deployed Executable - 64 bits
 * @version 0.1
 * @date 2024-01-07
 *
 * @copyright Copyright (c) Pradosh 2024-2026
 *
 */
#ifndef FWDE_H
#define FWDE_H

#include <basics.h>
#include <cc-asm.h>
#include <graphics.h>
#include <isr.h>
#include <meltdown.h>
#include <stdbool.h>

typedef struct {
    char signature[6];  // 0xCD + 0x31 + FWDE
    uint8 architecture; // 1 = 64 bits; 2 = 32 bits; 3 = 16 bits; 4 = 8 bits
    uint16 raw_size;    // size of just the executable part and not the header
    uint8 endian;       // 0 = error; 1 = little; 2 = big
} fwde_header;

typedef struct
{
    uint64 *fb_addr;
    uint64 width;
    uint64 height;
    uint64 pitch;
    void (*print)(cstring msg);
} kernel_data;

typedef void (*entry_function)(kernel_data *);

/**
 * @brief Verify a FWDE executable signature.
 *
 * @param signature Signature string to verify.
 * @return true if the signature is valid, otherwise false.
 */
bool verify_signature(const char *signature);

/**
 * @brief Process an interrupt frame raised while executing FWDE code.
 *
 * @param frame Interrupt state to process.
 */
void process_IFL(InterruptFrame *frame);

/**
 * @brief Load and execute a FWDE image.
 *
 * @param addr Address of the executable image.
 * @param data Kernel data passed to the executable.
 */
void execute_fwde(uint64 *addr, kernel_data *data);

#endif // FWDE_H