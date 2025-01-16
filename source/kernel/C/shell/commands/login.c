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

#include <commands/login.h>

char* usernames_total[MAX_USERS_ALLOWED];
char* passwords_total[MAX_USERS_ALLOWED];
int users_index = 0;

void create_user(char* name, char* password){
    if(users_index >= MAX_USERS_ALLOWED){
        error("Too many user accounts!", __FILE__);
        return;
    }

    
    for(int i=0; i<MAX_USERS_ALLOWED; i++){
        char* current_username = &usernames_total[i];
        if(strcmp(current_username, name) == 0){
            while(1);
            error("Already have an user with same name!", __FILE__);
            return;
        }
    }

    usernames_total[users_index] = name;
    passwords_total[users_index] = &password;
    users_index++;
    printf("Created a user named \'%s\'", name);
}

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

    if (strcmp(username, "root") == 0 && strcmp(password, "prad") == 0){
        return &username[0];
    }

    // for(int i=0; i<MAX_USERS_ALLOWED; i++){
    //     char* current_username = &usernames_total[i];
    //     char* current_password = &passwords_total[i];
    //     printf("username : %s", current_username);
    //     printf("password : %s", current_password);
    //     if(strcmp(current_username, &username[0]) == 0 && strcmp(current_password, password) == 0){
    //         return &username[0];
    //     }
    // }

    // If everything fails,
    return NULL;
}