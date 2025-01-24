/**
 * @file fwrfs.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <filesystems/fwrfs.h>
#include <heap.h>
#include <basics.h>
#include <debugger.h>

int is_validfilename(char c) {
    if ((c >= 'a' && c <= 'z') || 
        (c >= 'A' && c <= 'Z') || 
        (c >= '0' && c <= '9') ||
        (c == '.')) {
        return 1;
    } else {
        return 0;
    }
}

int is_valid_filename(const char* filename) {
    // Check for empty filename
    if (strlen_(filename) == 0) {
        return 0; 
    }

    for (int i = 0; filename[i] != '\0'; i++) {
        if (!is_validfilename(filename[i]) && filename[i] != '_') {
            return 0; 
        }
    }

    return 1; // Valid filename
}

int create_file(struct fwrfs* fs, const char* filename, const char* data) {
    if (fs->nfiles >= 100) {
        printf("Error: File system full.");
        return -1;
    }

    if (!is_valid_filename(filename)) {
        printf("Error: Invalid filename.");
        return -1;
    }

    strcpy(fs->files[fs->nfiles].name, filename);
    fs->files[fs->nfiles].data = (char*)malloc(strlen_(data) + 1);
    strcpy(fs->files[fs->nfiles].data, data);
    fs->nfiles++;

    return 0;
}

char* read_file(struct fwrfs* fs, const char* filename) {
    for (int i = 0; i < fs->nfiles; i++) {
        if (strcmp(fs->files[i].name, filename) == 0) {
            return fs->files[i].data;
        }
    }
    return NULL; 
}

int write_file(struct fwrfs* fs, const char* filename, const char* data) {
    bool done = false;
    for (int i = 0; i < fs->nfiles; i++) {
        if (strcmp(fs->files[i].name, filename) == 0) {
            strcpy(fs->files[fs->nfiles].data, data);
            done = true;
        }
    }
    if(done == false){
        create_file(fs, filename, data);
    }
    return 0; 
}

int delete_file(struct fwrfs* fs, const char* filename) {
    for (int i = 0; i < fs->nfiles; i++) {
        if (strcmp(fs->files[i].name, filename) == 0) {
            free(fs->files[i].data); 
            // Shift remaining files
            for (int j = i; j < fs->nfiles - 1; j++) {
                fs->files[j] = fs->files[j + 1];
            }
            fs->nfiles--;
            return 0;
        }
    }

    printf("rm: cannot remove \'%s\': No such file or directory", filename);
    return -1; 
}

int create_folder(struct fwrfs* fs, const char* foldername) {
    if (fs->nfolders >= 100) {
        printf("Error: File system full.");
        return -1;
    }

    strcpy(fs->folders[fs->nfolders].name, foldername);
    fs->folders[fs->nfolders].folders = NULL; // Initialize to NULL
    fs->folders[fs->nfolders].files = NULL;   // Initialize to NULL
    fs->folders[fs->nfolders].nfiles = 0;
    fs->nfolders++;

    return 0;
}

// Function to remove a folder (Simplified version - assumes empty folder)
int remove_folder(struct fwrfs* fs, const char* foldername) {
    for (int i = 0; i < fs->nfolders; i++) {
        if (strcmp(fs->folders[i].name, foldername) == 0) {
            // Shift remaining folders
            for (int j = i; j < fs->nfolders - 1; j++) {
                fs->folders[j] = fs->folders[j + 1];
            }
            fs->nfolders--;
            return 0;
        }
    }
    return -1; 
}

size_t calculate_memory_usage(struct fwrfs* fs) {
    size_t total_memory = 0;

    // Calculate memory used by files
    for (int i = 0; i < fs->nfiles; i++) {
        total_memory += strlen_(fs->files[i].name) + 1; // Account for null terminator
        total_memory += strlen_(fs->files[i].data) + 1; 
    }

    // Calculate memory used by folders (simplified - assumes no nested folders)
    for (int i = 0; i < fs->nfolders; i++) { 
        total_memory += strlen_(fs->folders[i].name) + 1; 
    }

    return total_memory;
}

void list_contents(struct fwrfs* fs) {
    
    if(fs->nfolders != 0 || fs->nfiles != 0){
        size_t total_memory = calculate_memory_usage(fs);
        printf("%sTotal Memory Used: %u bytes%s", "\x1b[38;2;128;128;128m", total_memory, reset_color);
    }

    for (int i = 0; i < fs->nfolders; i++) {
        printf("%s%s%s", yellow_color, fs->folders[i].name, reset_color);
    }

    for (int i = 0; i < fs->nfiles; i++) {
        printf("%s%s%s", green_color, fs->files[i].name,reset_color);
    }

}