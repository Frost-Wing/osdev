/**
 * @file login.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-14
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <commands/login.h>

int64 usernames_total[MAX_USERS_ALLOWED];
int64 passwords_total[MAX_USERS_ALLOWED];
int users_index = 0;

void create_user(int64 name_hash, int64 password_hash){
    if(users_index >= MAX_USERS_ALLOWED){
        error("Too many user accounts!", __FILE__);
        return;
    }

    for(int i=0; i<users_index; i++){   // only check existing users
        int64 current_username = usernames_total[i];
        if(current_username == name_hash){
            error("Already have an user with same name!", __FILE__);
            return;
        }
    }

    usernames_total[users_index] = name_hash;
    passwords_total[users_index] = password_hash;
    users_index++;
    printf("Created a user. id → 0x%X", (unsigned long long)name_hash);
}


void create_user_str(cstring name, cstring password){
    int64 name_hash = baranium_hash(name);
    int64 password_hash = baranium_hash(password);

    create_user(name_hash, password_hash);
}

char* login_request(){
    static char username[21]; // +1 for null terminator
    static char password[21];
    memset(username, 0, 21);
    memset(password, 0, 21);

    char temp;
    int i;

    for(int i = 0; i < 30; i++)
        putc('=');
    putc('\n');

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

    // Start hashing
    int64 username_hash = baranium_hash(username);
    int64 password_hash = baranium_hash(password);
    
    for(int i=0; i<users_index; i++){
        int64 current_username = usernames_total[i];
        int64 current_password = passwords_total[i];

        if(current_username == username_hash && current_password == password_hash){
            return &username[0];
        }
    }

    // If everything fails,
    return NULL;
}

int ask_password(const char* username){
    char temp;
    int i;

    int64 username_hash = baranium_hash(username);

    static char password[21];

    print("Password: ");
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
    putc('\n');

    int64 password_hash = baranium_hash(password);

    for(int i=0; i<users_index; i++){
        int64 current_username = usernames_total[i];
        int64 current_password = passwords_total[i];


        if(current_username == username_hash && current_password == password_hash){
            strcpy(password, "");
            return 0;
        }
    }

    strcpy(password, "");
    return 1;
}