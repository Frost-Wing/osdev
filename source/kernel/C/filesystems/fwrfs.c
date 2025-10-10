/**
 * @file fwrfs.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief An completely self-made RAMFS.
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <filesystems/fwrfs.h>
#include <heap.h>
#include <basics.h>
#include <strings.h>

struct fwrfs_folder* current_folder = NULL; // Points to current working folder

bool asciifilename(char c) {
    if ((c >= 'a' && c <= 'z') || 
        (c >= 'A' && c <= 'Z') || 
        (c >= '0' && c <= '9') ||
        (c == '.')|| c == '_') {
        return true;
    } else {
        return false;
    }
}

bool is_valid_filename(struct fwrfs_folder* parent, const char* filename) {
    if (strlen(filename) == 0) {
        return false; 
    }

    // check for naming scheme
    for (int i = 0; filename[i] != '\0'; i++) {
        if (!asciifilename(filename[i])) {
            return false; 
        }
    }

    //check for existing files with same name.
    for (int i = 0; i < current_folder->nfiles; i++) {
        if (strcmp(current_folder->files[i].name, filename) == 0) {
            return false; // already an file exsists with the name.
        }
    }

    //check for exisiting folders with same name
    if(find_folder(parent, filename) != NULL)
        return false; // folder already exists.

    return true; // Valid filename
}

struct fwrfs_folder* find_folder(struct fwrfs_folder* parent, const char* name) {
    if (!parent) return NULL;
    for (int i = 0; i < parent->nfiles + 100; i++) { // crude max
        struct fwrfs_folder* f = &parent->folders[i];
        if (strcmp(f->name, name) == 0) return f;
    }
    return NULL;
}

void init_fs(struct fwrfs* fs) {
    fs = (struct fwrfs*)kmalloc(sizeof(struct fwrfs));
    fs->nfiles = 0;
    fs->nfolders = 0;

    // Create root folder
    strcpy(fs->folders[0].name, "/");
    fs->folders[0].folders = NULL;
    fs->folders[0].files = NULL;
    fs->folders[0].nfiles = 0;
    fs->nfolders = 1;

    current_folder = &fs->folders[0];
}

int create_folder(struct fwrfs* fs, const char* foldername) {
    if (!current_folder) return -1;

    if(!is_valid_filename(fs, foldername)){
        printf("mkdir: file/folder already exists.");
        return 1;
    }

    struct fwrfs_folder* new_folder = kmalloc(sizeof(struct fwrfs_folder));
    strcpy(new_folder->name, foldername);
    new_folder->folders = NULL;
    new_folder->files = NULL;
    new_folder->nfiles = 0;
    new_folder->parent = current_folder;

    if (!current_folder->folders) {
        current_folder->folders = kmalloc(sizeof(struct fwrfs_folder));
    }
    int idx = current_folder->nfiles++;
    current_folder->folders[idx] = *new_folder;

    fs->nfolders++;
    return 0;
}

// cd .. now works
int cd(struct fwrfs* fs, const char* path) {
    if (strcmp(path, "/") == 0) {
        current_folder = &fs->folders[0];
        return 0;
    }

    if (strcmp(path, "..") == 0) {
        if (current_folder->parent) {
            current_folder = current_folder->parent;
            return 0;
        }

        printf("cd: already at \'/\'");
        return 2; // already at root
    }

    // search in current folder
    for (int i = 0; i < current_folder->nfiles; i++) {
        struct fwrfs_folder* f = &current_folder->folders[i];
        if (strcmp(f->name, path) == 0) {
            current_folder = f;
            return 0;
        }
    }
    printf("cd: no such directory: %s", path);
    return 1;
}

void pwd(struct fwrfs* fs) {
    printf("%s", get_pwd(fs));
}

char* get_pwd(struct fwrfs* fs) {
    if (!current_folder) return "/";

    char* path = (char*)kmalloc(256);
    path[0] = '\0';

    struct fwrfs_folder* f = current_folder;

    // Collect segments backwards
    while (f && f->parent) { // stop before root (root->parent == NULL)
        char tmp[256];
        strcpy(tmp, "/");
        strcat(tmp, f->name);
        strcat(tmp, path);
        strcpy(path, tmp);
        f = f->parent;
    }

    // If we never added anything, we’re at root
    if (path[0] == '\0') {
        strcpy(path, "/");
    }

    return path;
}



// Create file in current folder
int create_file(struct fwrfs* fs, const char* filename, const char* data) {
    if (!current_folder) return -1;

    if (!is_valid_filename(fs, filename)) {
        printf("Error: Invalid filename or file/folder already exists.");
        return -1;
    }

    struct fwrfs_file* file = kmalloc(sizeof(struct fwrfs_file));
    strcpy(file->name, filename);
    file->data = kmalloc(strlen(data) + 1);
    strcpy(file->data, data);

    if (!current_folder->files) {
        current_folder->files = kmalloc(sizeof(struct fwrfs_file));
    }

    current_folder->files[current_folder->nfiles++] = *file;
    fs->nfiles++; // global count
    return 0;
}

// Read file in current folder
char* read_file(struct fwrfs* fs, const char* filename) {
    if (!current_folder) return NULL;
    for (int i = 0; i < current_folder->nfiles; i++) {
        if (strcmp(current_folder->files[i].name, filename) == 0) {
            return current_folder->files[i].data;
        }
    }
    return NULL;
}

// Write file in current folder
int write_file(struct fwrfs* fs, const char* filename, const char* data) {
    if (!current_folder) return -1;
    for (int i = 0; i < current_folder->nfiles; i++) {
        if (strcmp(current_folder->files[i].name, filename) == 0) {
            kfree(current_folder->files[i].data);
            current_folder->files[i].data = kmalloc(strlen(data) + 1);
            strcpy(current_folder->files[i].data, data);
            return 0;
        }
    }
    return create_file(fs, filename, data);
}

// Delete file in current folder
int delete_file(struct fwrfs* fs, const char* filename) {
    if (!current_folder) return -1;
    for (int i = 0; i < current_folder->nfiles; i++) {
        if (strcmp(current_folder->files[i].name, filename) == 0) {
            kfree(current_folder->files[i].data);
            for (int j = i; j < current_folder->nfiles - 1; j++) {
                current_folder->files[j] = current_folder->files[j + 1];
            }
            current_folder->nfiles--;
            fs->nfiles--;
            return 0;
        }
    }
    printf("rm: cannot remove '%s': No such file", filename);
    return -1;
}

// List current folder contents
void list_contents(struct fwrfs* fs) {
    if (!current_folder) return;

    if (current_folder->folders) {
        for (int i = 0; i < current_folder->nfiles; i++) {
            printf("%s%s%s", yellow_color, current_folder->folders[i].name, reset_color);
        }
    }
    if(current_folder->files){
        for (int i = 0; i < current_folder->nfiles; i++) {
            printf("%s%s%s", green_color, current_folder->files[i].name, reset_color);
        }
    }
}
