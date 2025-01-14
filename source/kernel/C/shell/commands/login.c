/**
 * @file login.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <basics.h>

char* login_request(){
    static char username[21]; // +1 for null terminator
    static char password[21];
    memset(username, 0, 21);
    memset(password, 0, 21);

    char temp;
    int i;

    for(int i = 0; i < 30; i++)
        __putc('=');
    __putc('\n');

    print("Username: ");
    i = 0;
    while ((temp = getc()) != '\n' && i < 20) {
        if (temp == 0)
            continue;
        if (temp == '\b')
        {
            if (i == 0) continue;
            username[i] = 0;
            i--;
            putc('\b');
        }
        else
        {
            username[i] = temp;
            i++;
            putc(temp);
        }
    }
    username[i] = '\0';

    print("\nPassword: ");
    i = 0;
    while ((temp = getc()) != '\n' && i < 20) {
        if (temp == 0)
            continue;
        if (temp == '\b')
        {
            if (i == 0) continue;
            password[i] = 0;
            i--;
            putc('\b');
        }
        else
        {
            password[i] = temp;
            i++;
            putc('*');
        }
    }
    password[i] = '\0';

    __putc('\n');
    
    if (strcmp(username, "root") == 0 && strcmp(password, "prad") == 0)
        return &username[0];
    else
        return NULL;

}