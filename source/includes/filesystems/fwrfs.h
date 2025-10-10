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
    int nfolders;
    struct fwrfs_folder* parent;  // <-- NEW: for pwd and cd ..
};

struct fwrfs
{
    struct fwrfs_file files[100];
    struct fwrfs_folder folders[100];
    int nfiles;
    int nfolders;
};

/**
 * @brief Checks whether the characters in the file name follow "Good ASCII"
 * 
 * @param c The character to be checked.
 * @return int status.
 */
bool asciifilename(char c);

/**
 * @brief Checks whether the file/folder is valid under conditions.
 * 
 * @param parent Parent filesystem structure pointer.
 * @param filename filename duh?
 * @return true 
 * @return false 
 */
bool is_valid_filename(struct fwrfs_folder* parent, const char* filename);

/**
 * @brief Helper: find folder by name in a parent folder
 * 
 * @param parent Parent filesystem structure pointer.
 * @param name folder name.
 * @return struct fwrfs_folder* The pointer pointing to the folder in memory address.
 */
struct fwrfs_folder* find_folder(struct fwrfs_folder* parent, const char* name);

char* get_pwd(struct fwrfs* fs);