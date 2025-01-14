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
    char username[21]; // +1 for null terminator
    char password[21];

    char temp;
    int i;

    for(int i = 0; i < 30; i++)
        __putc('=');
    __putc('\n');
    
    warn("DON'T USE BACKSPACE!!", __FILE__);

    print("Username: ");
    i = 0;
    while ((temp = getc()) != 0x1c && i < 20) {
        if (temp == '\b')
        {
            if (i == 0) continue;
            username[i] = 0;
            i--;
            __putc('\b');
        }
        else
        {
            username[i] = temp;
            i++;
            __putc(temp);
        }
    }
    username[i] = '\0';

    print("\nPassword: ");
    i = 0;
    while ((temp = getc()) != 0x1c && i < 20) {
        if (temp == '\b')
        {
            if (i == 0) continue;
            password[i] = 0;
            i--;
        }
        else
        {
            password[i] = temp;
            i++;
            __putc('*');
        }
    }
    password[i] = '\0';

    __putc('\n');
    
    if (strcmp(username, "root") == 0 && strcmp(password, "prad") == 0) {
        memset(username, 0, 21);
        memset(password, 0, 21);
        return username; // I know this won't give the username.. need to fix!
    } else {
        return "";
    }
}