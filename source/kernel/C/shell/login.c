/**
 * @file login.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-01-14
 *
 * @copyright Copyright (c) Pradosh 2025-2026
 *
 */

#include <commands/login.h>
#include <algorithms/password.h>
#include <cc-asm.h>
#include <fb.h>
#include <filesystems/vfs.h>
#include <graphics.h>
#include <keyboard.h>
#include <memory.h>
#include <tty.h>

#define LOGIN_AUTH_PATH "/etc/fwauth.dat"
#define LOGIN_AUTH_TEMP_PATH "/etc/fwauth.tmp"
#define LOGIN_KDF_ITERATIONS 100000U

typedef struct __attribute__((packed)) {
    char magic[8];
    uint32_t version;
    uint32_t iterations;
    uint64_t username_hash;
    uint8_t salt[PASSWORD_SALT_SIZE];
    uint8_t password_hash[PASSWORD_HASH_SIZE];
} login_auth_record_t;

_Static_assert(sizeof(login_auth_record_t) == 72, "Unexpected auth record size");

extern struct flanterm_context *ft_ctx;

uint64 usernames_total[MAX_USERS_ALLOWED];
uint8 passwords_total[MAX_USERS_ALLOWED][PASSWORD_HASH_SIZE];
uint8 password_salts[MAX_USERS_ALLOWED][PASSWORD_SALT_SIZE];
int users_index = 0;

static bool login_read_line(const char *prompt, char *buffer, size_t capacity,
    bool hide_input) {
    linux_termios_t original_termios;
    if (!tty_get_termios(&original_termios))
        return false;

    linux_termios_t input_termios = original_termios;
    input_termios.c_lflag &= (uint32_t)~LINUX_ECHO;
    if (!tty_set_termios(&input_termios, false))
        return false;

    size_t length = 0;

    print(prompt);
    while (true) {
        int key = getc();
        if (key == 0 || key == CUR_UP || key == CUR_DOWN)
            continue;

        char ch = (char)key;
        if (ch == '\n' || ch == '\r')
            break;
        if (ch == '\b') {
            if (length) {
                buffer[--length] = '\0';
                print("\b \b");
            }
            continue;
        }
        if (ch < 0x20 || ch > 0x7e || length + 1 >= capacity)
            continue;

        buffer[length++] = ch;
        buffer[length] = '\0';
        if (hide_input)
            putc('*');
        else
            putc(ch);
    }
    buffer[length] = '\0';
    putc('\n');
    tty_flush_input();
    return tty_set_termios(&original_termios, false);
}

static void login_clear_secret(char *secret, size_t length) {
    volatile char *p = secret;
    while (length--)
        *p++ = 0;
}

static void login_add_user(uint64 name_hash, const uint8_t salt[PASSWORD_SALT_SIZE],
    const uint8_t password_hash[PASSWORD_HASH_SIZE]) {
    if (users_index >= MAX_USERS_ALLOWED) {
        error("Too many user accounts!", __FILE__);
        return;
    }

    for (int i = 0; i < users_index; i++) { // only check existing users
        uint64 current_username = usernames_total[i];
        if (current_username == name_hash) {
            error("Already have an user with same name!", __FILE__);
            return;
        }
    }

    usernames_total[users_index] = name_hash;
    memcpy(password_salts[users_index], salt, PASSWORD_SALT_SIZE);
    memcpy(passwords_total[users_index], password_hash, PASSWORD_HASH_SIZE);
    users_index++;
}

static bool login_hash_matches(const uint8_t a[PASSWORD_HASH_SIZE],
    const uint8_t b[PASSWORD_HASH_SIZE]) {
    uint8_t difference = 0;
    for (size_t i = 0; i < PASSWORD_HASH_SIZE; ++i)
        difference |= a[i] ^ b[i];
    return difference == 0;
}

static bool login_auth_filesystem_writable(void) {
    vfs_mount_res_t mount;
    if (vfs_resolve_mount(LOGIN_AUTH_PATH, &mount) != 0)
        return false;

    return mount.mnt->type == FS_FAT16 || mount.mnt->type == FS_FAT32 ||
        mount.mnt->type == FS_EXT2;
}

static int login_read_auth_record(login_auth_record_t *record) {
    vfs_file_t file;
    int opened = vfs_open(LOGIN_AUTH_PATH, VFS_RDONLY, &file);
    if (opened != 0)
        return opened == -6 ? 0 : -1;

    int read = vfs_read(&file, (uint8_t *)record, sizeof(*record));
    uint8_t extra;
    int trailing = read == (int)sizeof(*record)
        ? vfs_read(&file, &extra, sizeof(extra))
        : -1;
    vfs_close(&file);

    if (read != (int)sizeof(*record) || trailing != 0)
        return -1;
    return 1;
}

static bool login_write_auth_record(const login_auth_record_t *record) {
    vfs_file_t file;
    if (vfs_open(LOGIN_AUTH_TEMP_PATH,
            VFS_WRONLY | VFS_CREATE | VFS_TRUNC, &file) != 0)
        return false;

    int written = vfs_write(&file, (const uint8_t *)record, sizeof(*record));
    vfs_close(&file);
    if (written != (int)sizeof(*record) || vfs_sync(true) != 0)
        return false;

    if (vfs_mv(LOGIN_AUTH_TEMP_PATH, LOGIN_AUTH_PATH) != 0 ||
        vfs_sync(true) != 0)
        return false;

    login_auth_record_t verify;
    return login_read_auth_record(&verify) == 1 &&
        memcmp(&verify, record, sizeof(verify)) == 0;
}

static void login_make_salt(uint64_t username_hash, uint8_t salt[PASSWORD_SALT_SIZE]) {
    uint64_t first = rdtsc64();
    uint64_t second = rdtsc64() ^ username_hash ^ (uint64_t)(uintptr_t)salt;
    memcpy(salt, &first, sizeof(first));
    memcpy(salt + sizeof(first), &second, sizeof(second));
}

static int login_load_existing_account(void) {
    login_auth_record_t record;
    int result = login_read_auth_record(&record);
    if (result != 1)
        return result;

    static const char expected_magic[8] = {'F', 'W', 'A', 'U', 'T', 'H', '1', '\0'};
    if (memcmp(record.magic, expected_magic, sizeof(expected_magic)) != 0 ||
        record.version != 1 || record.iterations != LOGIN_KDF_ITERATIONS ||
        record.username_hash != baranium_hash("root")) {
        error("Stored account data is invalid; refusing password setup.", __FILE__);
        return -1;
    }

    login_add_user(record.username_hash, record.salt, record.password_hash);
    memset(&record, 0, sizeof(record));
    return users_index == 1 ? 1 : -1;
}

int login_setup_first_user(void) {
    if (users_index != 0)
        return 0;

    if (!login_auth_filesystem_writable()) {
        error("Root filesystem is not writable; cannot store the root password.", __FILE__);
        return -1;
    }

    int loaded = login_load_existing_account();
    if (loaded == 1)
        return 0;
    if (loaded < 0) {
        error("Cannot load stored root credentials; refusing insecure fallback.", __FILE__);
        return -1;
    }

    char password[MAX_PASSWORD_LENGTH + 1] = {0};
    char confirmation[MAX_PASSWORD_LENGTH + 1] = {0};
    uint64 username_hash = baranium_hash("root");
    login_auth_record_t record = {0};
    static const char magic[8] = {'F', 'W', 'A', 'U', 'T', 'H', '1', '\0'};

    print("==========================\n");
    info("No stored root account. Set a unique root password for this installation.", __FILE__);
    if (!login_read_line("New root password (12-40 characters): ",
            password, sizeof(password), true) ||
        !login_read_line("Confirm password: ", confirmation,
            sizeof(confirmation), true)) {
        error("Unable to configure terminal input for password setup.", __FILE__);
        login_clear_secret(password, sizeof(password));
        login_clear_secret(confirmation, sizeof(confirmation));
        return -1;
    }

    if (strlen(password) < 12 || strcmp(password, confirmation) != 0) {
        error("Password setup failed: use 12-40 characters and confirm it exactly.", __FILE__);
        login_clear_secret(password, sizeof(password));
        login_clear_secret(confirmation, sizeof(confirmation));
        return -1;
    }

    memcpy(record.magic, magic, sizeof(magic));
    record.version = 1;
    record.iterations = LOGIN_KDF_ITERATIONS;
    record.username_hash = username_hash;
    login_make_salt(username_hash, record.salt);
    password_derive_key((const uint8_t *)password, strlen(password),
        record.salt, record.iterations, record.password_hash);

    bool saved = login_write_auth_record(&record);
    login_clear_secret(password, sizeof(password));
    login_clear_secret(confirmation, sizeof(confirmation));
    if (!saved) {
        memset(&record, 0, sizeof(record));
        error("Could not persist root credentials; refusing login without them.", __FILE__);
        return -1;
    }

    login_add_user(username_hash, record.salt, record.password_hash);
    memset(&record, 0, sizeof(record));
    return users_index == 1 ? 0 : -1;
}

int login_request(char *userbuf, int max) {
    static char username[MAX_USERNAME_LENGTH + 1];
    static char password[MAX_PASSWORD_LENGTH + 1];

    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));

    if (!userbuf || max <= 0)
        return -1;

    for (int j = 0; j < 32; j++)
        putc('=');
    putc('\n');

    if (!login_read_line("Username: ", username, sizeof(username), false) ||
        !login_read_line("Password: ", password, sizeof(password), true)) {
        error("Unable to configure terminal input for login.", __FILE__);
        login_clear_secret(password, sizeof(password));
        memset(username, 0, sizeof(username));
        return -1;
    }

    uint64 username_hash = baranium_hash(username);
    for (int i = 0; i < users_index; i++) {
        uint8_t password_hash[PASSWORD_HASH_SIZE];
        password_derive_key((const uint8_t *)password, strlen(password),
            password_salts[i], LOGIN_KDF_ITERATIONS, password_hash);
        bool matches = usernames_total[i] == username_hash &&
            login_hash_matches(passwords_total[i], password_hash);
        memset(password_hash, 0, sizeof(password_hash));
        if (matches) {

            snprintf(userbuf, (size_t)max, "%s", username);
            login_clear_secret(password, sizeof(password));
            memset(username, 0, sizeof(username));
            return 0;
        }
    }

    login_clear_secret(password, sizeof(password));
    memset(username, 0, sizeof(username));
    return -1;
}

int ask_password(const char *username) {
    uint64 username_hash = baranium_hash(username);
    static char password[MAX_PASSWORD_LENGTH + 1];
    memset(password, 0, sizeof(password));
    if (!login_read_line("Password: ", password, sizeof(password), true)) {
        error("Unable to configure terminal input for password entry.", __FILE__);
        login_clear_secret(password, sizeof(password));
        return 1;
    }
    putc('\n');

    for (int i = 0; i < users_index; i++) {
        uint64 current_username = usernames_total[i];
        uint8_t password_hash[PASSWORD_HASH_SIZE];
        password_derive_key((const uint8_t *)password, strlen(password),
            password_salts[i], LOGIN_KDF_ITERATIONS, password_hash);
        bool matches = current_username == username_hash &&
            login_hash_matches(passwords_total[i], password_hash);
        memset(password_hash, 0, sizeof(password_hash));
        if (matches) {
            login_clear_secret(password, sizeof(password));
            return 0;
        }
    }

    login_clear_secret(password, sizeof(password));
    return 1;
}
