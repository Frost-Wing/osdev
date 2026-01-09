/**
 * @file cat.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Basic linux cat command.
 * @version 0.1
 * @date 2025-10-07
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/commands.h>
#include <filesystems/vfs.h>
#include <basics.h>

#define CAT_BUF_SIZE 512

int cmd_cat(int argc, char** argv)
{
    if (argc < 2) {
        printf("cat: missing file operand");
        return 1;
    }

    uint8_t buf[CAT_BUF_SIZE];
    int j;

    for (int i = 1; i < argc; i++) {
        vfs_file_t file;

        /* Open file */
        if (vfs_open(argv[i], VFS_RDONLY, &file) != 0) {
            printf("cat: %s: no such file or directory", argv[i]);
            break;
        }

        /* Read loop */
        while (1) {
            int r = vfs_read(&file, buf, CAT_BUF_SIZE);
            if (r < 0) {
                printf("cat: %s: read error", argv[i]);
                break;
            }

            if (r == 0)
                break; /* EOF */

            for (j = 0; j < r && buf != null; j++)
                printfnoln("%c", buf[j]);
        }

        if(j != 0 && buf != null)
            if(buf[j-1] != '\n')
                print("\n");

        vfs_close(&file);
    }
    
    return 0;
}
