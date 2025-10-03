/**
 * @file tarball.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The header for tarball.
 * @version 0.1
 * @date 2023-12-30
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>


/**
 * @brief Size of a tar block in bytes.
 */
#define block_size 512

/**
 * @brief Structure representing a TAR archive header.
 *
 * This structure corresponds to the standard TAR header format.
 */
struct tarball_header {
    char name[100];
    char mode[8];
    char owner[8];
    char group[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char owner_name[32];
    char group_name[32];
    char dev_major[8];
    char dev_minor[8];
    char prefix[155];
};

/**
 * @brief Extracts a TAR archive from a memory location.
 *
 * This function reads the TAR archive located at the memory address
 * `tarball_addr` and reads all contained files and directories
 * to the current working directory.
 *
 * @param tarball_addr Pointer to the memory location where the TAR archive is stored.
 */
void extract_tarball(int64* tarball_addr);