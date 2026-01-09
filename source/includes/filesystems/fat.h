/**
 * @file fat.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The general header which applies to any FAT file system.
 * @version 0.1
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */

#ifndef FAT_H
#define FAT_H

typedef enum {
    FAT_OK = 0,

    // generic
    FAT_ERR_IO,
    FAT_ERR_INVALID,
    FAT_ERR_NOT_FOUND,
    FAT_ERR_EXISTS,
    FAT_ERR_NOT_DIR,
    FAT_ERR_IS_DIR,
    FAT_ERR_NO_SPACE,
    FAT_ERR_NAME_INVALID,
    FAT_ERR_NOT_EMPTY,
    FAT_ERR_READ,

    // filesystem corruption (meltdown-worthy)
    FAT_ERR_CORRUPT,
    FAT_ERR_FAT_LOOP,
    FAT_ERR_BAD_CLUSTER,
} fat_err_t;

#endif