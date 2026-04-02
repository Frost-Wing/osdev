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
#include <keyboard.h>

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
    printf("Created a user with id → 0x%X", (int64)name_hash);
    info("Done creating an user!", __FILE__);
}


void create_user_str(cstring name, cstring password){
    int64 name_hash = baranium_hash(name);
    int64 password_hash = baranium_hash(password);

    create_user(name_hash, password_hash);
}

int login_request(char* userbuf, int max){
    static char username[21];
    static char password[21];

    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));

    int k;
    char temp;
    int i;

    for(int j = 0; j < 32; j++)
        putc('=');
    putc('\n');

    /* -------- USERNAME -------- */

    print("Username: ");
    i = 0;

    while(i < 20){
        k = getc();

        if (k == 0)
            continue;

        temp = (char)k;

        if(temp == '\n' || temp == '\r')
            break;

        if(temp == '\b'){
            if(i == 0) continue;

            i--;
            username[i] = 0;
            putc('\b');
            continue;
        }

        username[i++] = temp;
    }

    username[i] = '\0';

    /* -------- PASSWORD -------- */

    print("\nPassword: ");
    i = 0;

    while(i < 20){
        k = getc();

        if (k == 0)
            continue;
        
        temp = (char)k;

        if(temp == '\n' || temp == '\r')
            break;

        if(temp == '\b'){
            if(i == 0) continue;

            i--;
            password[i] = 0;
            putc('\b');
            continue;
        }

        password[i++] = temp;
    }

    password[i] = '\0';

    vputc('\n');

    int64 username_hash = baranium_hash(username);
    int64 password_hash = baranium_hash(password);

    for(int i = 0; i < users_index; i++){
        if(usernames_total[i] == username_hash &&
           passwords_total[i] == password_hash){

            int j = 0;
            while(username[j] && j < max-1){
                userbuf[j] = username[j];
                j++;
            }

            userbuf[j] = '\0';

            return 0;
        }
    }

    return -1;
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