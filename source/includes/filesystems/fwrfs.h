/**
 * @file fwrfs.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief FrostWing RAM FileSystem.
 * @version 0.1
 * @date 2025-01-21
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <basics.h>
#include <strings.h>
#include <graphics.h>

struct fwrfs_file {
    char name[30]; // with extension and '.' uses one byte.
    char* data;
};

struct fwrfs_folder{
    char name[20];
    struct fwrfs_folder* folders;
    struct fwrfs_file* files;
    int nfiles;
};

struct fwrfs
{
    struct fwrfs_file files[100];
    struct fwrfs_folder folders[100];
    int nfiles;
    int nfolders;
};
