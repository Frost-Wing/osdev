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
#include <strings.h>
#include <tty.h>

#define LOGIN_PASSWD_PATH "/etc/passwd"
#define LOGIN_SHADOW_PATH "/etc/shadow"
#define LOGIN_PASSWD_TEMP_PATH "/etc/passwd.tmp"
#define LOGIN_SHADOW_TEMP_PATH "/etc/shadow.tmp"
#define LOGIN_LEGACY_AUTH_PATH "/etc/fwauth.dat"
#define LOGIN_KDF_ITERATIONS 100000U
#define LOGIN_DB_MAX_BYTES 4096U
#define LOGIN_SHADOW_HASH_PREFIX "$fwauth$100000$"
#define LOGIN_SHADOW_SALT_HEX_SIZE (PASSWORD_SALT_SIZE * 2)
#define LOGIN_SHADOW_HASH_HEX_SIZE (PASSWORD_HASH_SIZE * 2)

typedef struct __attribute__((packed)) {
    char magic[8];
    uint32_t version;
    uint32_t iterations;
    uint64_t username_hash;
    uint8_t salt[PASSWORD_SALT_SIZE];
    uint8_t password_hash[PASSWORD_HASH_SIZE];
} login_auth_record_t;

typedef struct {
    char username[MAX_USERNAME_LENGTH + 1];
    uint8_t salt[PASSWORD_SALT_SIZE];
    uint8_t password_hash[PASSWORD_HASH_SIZE];
    bool usable;
} login_shadow_entry_t;

_Static_assert(sizeof(login_auth_record_t) == 72, "Unexpected auth record size");

extern struct flanterm_context *ft_ctx;

uint64 usernames_total[MAX_USERS_ALLOWED];
uint8 passwords_total[MAX_USERS_ALLOWED][PASSWORD_HASH_SIZE];
uint8 password_salts[MAX_USERS_ALLOWED][PASSWORD_SALT_SIZE];
int users_index = 0;

static char passwd_contents[LOGIN_DB_MAX_BYTES];
static char shadow_contents[LOGIN_DB_MAX_BYTES];
static login_shadow_entry_t shadow_entries[MAX_USERS_ALLOWED];

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

static bool login_add_user(uint64 name_hash, const uint8_t salt[PASSWORD_SALT_SIZE],
    const uint8_t password_hash[PASSWORD_HASH_SIZE]) {
    if (users_index >= MAX_USERS_ALLOWED) {
        error("Too many user accounts!", __FILE__);
        return false;
    }

    for (int i = 0; i < users_index; i++) { // only check existing users
        uint64 current_username = usernames_total[i];
        if (current_username == name_hash) {
            error("Already have an user with same name!", __FILE__);
            return false;
        }
    }

    usernames_total[users_index] = name_hash;
    memcpy(password_salts[users_index], salt, PASSWORD_SALT_SIZE);
    memcpy(passwords_total[users_index], password_hash, PASSWORD_HASH_SIZE);
    users_index++;
    return true;
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
    if (vfs_resolve_mount(LOGIN_PASSWD_PATH, &mount) != 0)
        return false;

    return mount.mnt->type == FS_FAT16 || mount.mnt->type == FS_FAT32 ||
        mount.mnt->type == FS_EXT2;
}

static int login_read_auth_record(login_auth_record_t *record) {
    vfs_file_t file;
    int opened = vfs_open(LOGIN_LEGACY_AUTH_PATH, VFS_RDONLY, &file);
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

static bool login_set_private_file_mode(const char *path) {
    vfs_mount_res_t mount;
    if (vfs_resolve_mount(path, &mount) != 0)
        return false;
    if (mount.mnt->type != FS_EXT2)
        return true;

    ext2_fs_t *fs = (ext2_fs_t *)mount.mnt->fs;
    uint32_t inode_number;
    ext2_inode_t inode;
    int found = ext2_find_path(fs, mount.rel_path, &inode_number, &inode);
    if (found == EXT2_ERR_NOT_FOUND)
        return true;
    if (found != EXT2_OK)
        return false;
    if ((inode.i_mode & 07777U) == 0600)
        return true;

    return ext2_chmod_inode(fs, inode_number, 0600, &inode) == EXT2_OK &&
        ext2_sync(fs) == EXT2_OK;
}

static int login_read_text_file(const char *path, char *buffer, size_t capacity) {
    vfs_file_t file;
    int opened = vfs_open(path, VFS_RDONLY, &file);
    if (opened != 0)
        return opened == -6 ? 0 : -1;

    size_t total = 0;
    int result = 1;
    while (total < capacity - 1) {
        int read = vfs_read(&file, (uint8_t *)buffer + total,
            (uint32_t)(capacity - 1 - total));
        if (read < 0) {
            result = -1;
            break;
        }
        if (read == 0)
            break;
        total += (size_t)read;
    }
    if (result == 1 && total == capacity - 1) {
        uint8_t extra;
        if (vfs_read(&file, &extra, sizeof(extra)) != 0)
            result = -1;
    }
    vfs_close(&file);

    if (result != 1)
        return result;
    for (size_t i = 0; i < total; ++i) {
        if (buffer[i] == '\0')
            return -1;
    }
    buffer[total] = '\0';
    return 1;
}

static bool login_write_text_file(const char *path, const char *contents) {
    vfs_file_t file;
    if (vfs_open(path, VFS_WRONLY | VFS_CREATE | VFS_TRUNC, &file) != 0)
        return false;

    if (strcmp(path, LOGIN_SHADOW_TEMP_PATH) == 0 &&
        !login_set_private_file_mode(path)) {
        vfs_close(&file);
        return false;
    }

    size_t length = strlen(contents);
    int written = vfs_write(&file, (const uint8_t *)contents, (uint32_t)length);
    vfs_close(&file);
    return written == (int)length && vfs_sync(true) == 0;
}

static void login_make_salt(uint64_t username_hash, uint8_t salt[PASSWORD_SALT_SIZE]) {
    uint64_t first = rdtsc64();
    uint64_t second = rdtsc64() ^ username_hash ^ (uint64_t)(uintptr_t)salt;
    memcpy(salt, &first, sizeof(first));
    memcpy(salt + sizeof(first), &second, sizeof(second));
}

static bool login_valid_username(const char *username) {
    size_t length = strlen(username);
    if (length == 0 || length > MAX_USERNAME_LENGTH)
        return false;
    for (size_t i = 0; i < length; ++i) {
        char ch = username[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' ||
                ch == '.'))
            return false;
    }
    return true;
}

static int login_hex_value(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

static bool login_decode_hex(const char *hex, uint8_t *output, size_t output_size) {
    for (size_t i = 0; i < output_size; ++i) {
        int high = login_hex_value(hex[i * 2]);
        int low = login_hex_value(hex[i * 2 + 1]);
        if (high < 0 || low < 0)
            return false;
        output[i] = (uint8_t)((high << 4) | low);
    }
    return true;
}

static void login_encode_hex(const uint8_t *bytes, size_t length, char *output) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < length; ++i) {
        output[i * 2] = hex[bytes[i] >> 4];
        output[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    output[length * 2] = '\0';
}

static bool login_parse_shadow_hash(const char *value, uint8_t salt[PASSWORD_SALT_SIZE],
    uint8_t password_hash[PASSWORD_HASH_SIZE]) {
    const size_t prefix_length = sizeof(LOGIN_SHADOW_HASH_PREFIX) - 1;
    if (strncmp(value, LOGIN_SHADOW_HASH_PREFIX, prefix_length) != 0)
        return false;

    const char *salt_hex = value + prefix_length;
    if (strlen(salt_hex) != LOGIN_SHADOW_SALT_HEX_SIZE + 1 +
            LOGIN_SHADOW_HASH_HEX_SIZE ||
        salt_hex[LOGIN_SHADOW_SALT_HEX_SIZE] != '$')
        return false;

    return login_decode_hex(salt_hex, salt, PASSWORD_SALT_SIZE) &&
        login_decode_hex(salt_hex + LOGIN_SHADOW_SALT_HEX_SIZE + 1,
            password_hash, PASSWORD_HASH_SIZE);
}

static bool login_split_fields(char *line, char **fields, size_t expected_count) {
    size_t count = 1;
    fields[0] = line;
    for (char *p = line; *p; ++p) {
        if (*p != ':')
            continue;
        if (count >= expected_count)
            return false;
        *p = '\0';
        fields[count++] = p + 1;
    }
    return count == expected_count;
}

static int login_load_shadow_entries(char *contents) {
    memset(shadow_entries, 0, sizeof(shadow_entries));
    size_t count = 0;
    char *line = contents;
    while (*line) {
        char *next = strchr(line, '\n');
        if (next)
            *next = '\0';
        size_t length = strlen(line);
        if (length && line[length - 1] == '\r')
            line[length - 1] = '\0';

        if (*line) {
            char *username_end = strchr(line, ':');
            if (!username_end)
                return -1;
            *username_end++ = '\0';
            char *hash_end = strchr(username_end, ':');
            if (!hash_end)
                return -1;
            *hash_end = '\0';

            if (!login_valid_username(line))
                return -1;

            login_shadow_entry_t parsed = {0};
            parsed.usable = login_parse_shadow_hash(username_end,
                parsed.salt, parsed.password_hash);
            if (parsed.usable) {
                if (count >= MAX_USERS_ALLOWED)
                    return -1;
                for (size_t i = 0; i < count; ++i) {
                    if (strcmp(shadow_entries[i].username, line) == 0)
                        return -1;
                }
                strcpy(parsed.username, line);
                shadow_entries[count++] = parsed;
            }
        }
        if (!next)
            break;
        line = next + 1;
    }
    return (int)count;
}

static int login_load_passwd_entries(char *contents, int shadow_count) {
    bool root_loaded = false;
    bool root_seen = false;
    char *line = contents;
    while (*line) {
        char *next = strchr(line, '\n');
        if (next)
            *next = '\0';
        size_t length = strlen(line);
        if (length && line[length - 1] == '\r')
            line[length - 1] = '\0';

        if (*line) {
            char *fields[7];
            if (!login_split_fields(line, fields, 7) ||
                !login_valid_username(fields[0])) {
                return -1;
            }

            if (strcmp(fields[0], "root") == 0) {
                if (root_seen || strcmp(fields[2], "0") != 0 ||
                    strcmp(fields[3], "0") != 0)
                    return -1;
                root_seen = true;
            }

            if (strcmp(fields[1], "x") == 0) {
                login_shadow_entry_t *credential = NULL;
                for (int i = 0; i < shadow_count; ++i) {
                    if (strcmp(shadow_entries[i].username, fields[0]) == 0) {
                        credential = &shadow_entries[i];
                        break;
                    }
                }
                if (credential && credential->usable) {
                    if (!login_add_user(baranium_hash(fields[0]), credential->salt,
                            credential->password_hash))
                        return -1;
                    if (strcmp(fields[0], "root") == 0)
                        root_loaded = true;
                }
            }
        }
        if (!next)
            break;
        line = next + 1;
    }
    return root_loaded ? 1 : -1;
}

static bool login_write_account_database(const login_auth_record_t *record) {
    char salt_hex[LOGIN_SHADOW_SALT_HEX_SIZE + 1];
    char hash_hex[LOGIN_SHADOW_HASH_HEX_SIZE + 1];
    char shadow_text[256];
    char passwd_text[128];
    login_encode_hex(record->salt, sizeof(record->salt), salt_hex);
    login_encode_hex(record->password_hash, sizeof(record->password_hash), hash_hex);

    int shadow_length = snprintf(shadow_text, sizeof(shadow_text),
        "root:" LOGIN_SHADOW_HASH_PREFIX "%s$%s::0:99999:7:::\n",
        salt_hex, hash_hex);
    int passwd_length = snprintf(passwd_text, sizeof(passwd_text),
        "root:x:0:0:root:/root:/bin/sh\n");
    if (shadow_length <= 0 || (size_t)shadow_length >= sizeof(shadow_text) ||
        passwd_length <= 0 || (size_t)passwd_length >= sizeof(passwd_text))
        return false;

    if (!login_write_text_file(LOGIN_SHADOW_TEMP_PATH, shadow_text) ||
        vfs_mv(LOGIN_SHADOW_TEMP_PATH, LOGIN_SHADOW_PATH) != 0 ||
        vfs_sync(true) != 0)
        return false;
    if (!login_write_text_file(LOGIN_PASSWD_TEMP_PATH, passwd_text) ||
        vfs_mv(LOGIN_PASSWD_TEMP_PATH, LOGIN_PASSWD_PATH) != 0 ||
        vfs_sync(true) != 0)
        return false;
    return true;
}

static int login_load_existing_accounts(void) {
    int passwd_result = login_read_text_file(LOGIN_PASSWD_PATH,
        passwd_contents, sizeof(passwd_contents));
    if (!login_set_private_file_mode(LOGIN_SHADOW_PATH)) {
        error("Could not enforce mode 0600 on /etc/shadow.", __FILE__);
        return -1;
    }
    int shadow_result = login_read_text_file(LOGIN_SHADOW_PATH,
        shadow_contents, sizeof(shadow_contents));

    if (passwd_result == 0 && shadow_result == 0)
        return 0;
    if (passwd_result != 1 || shadow_result != 1) {
        error("Account database is incomplete; refusing password setup.", __FILE__);
        return -1;
    }

    users_index = 0;
    int shadow_count = login_load_shadow_entries(shadow_contents);
    if (shadow_count < 0 ||
        login_load_passwd_entries(passwd_contents, shadow_count) != 1) {
        users_index = 0;
        error("Stored account database is invalid or has no usable root account.", __FILE__);
        return -1;
    }
    return 1;
}

static int login_migrate_legacy_account(void) {
    login_auth_record_t record;
    int result = login_read_auth_record(&record);
    if (result != 1)
        return result;

    static const char expected_magic[8] = {'F', 'W', 'A', 'U', 'T', 'H', '1', '\0'};
    if (memcmp(record.magic, expected_magic, sizeof(expected_magic)) != 0 ||
        record.version != 1 || record.iterations != LOGIN_KDF_ITERATIONS ||
        record.username_hash != baranium_hash("root")) {
        memset(&record, 0, sizeof(record));
        error("Stored legacy account data is invalid; refusing password setup.", __FILE__);
        return -1;
    }

    if (!login_write_account_database(&record)) {
        memset(&record, 0, sizeof(record));
        error("Could not migrate root credentials to /etc/passwd and /etc/shadow.", __FILE__);
        return -1;
    }

    int loaded = login_load_existing_accounts();
    memset(&record, 0, sizeof(record));
    if (loaded != 1)
        return -1;
    if (vfs_unlink(LOGIN_LEGACY_AUTH_PATH) != 0)
        error("Migrated credentials, but could not remove /etc/fwauth.dat.", __FILE__);
    return 1;
}

int login_setup_first_user(void) {
    if (users_index != 0)
        return 0;

    if (!login_auth_filesystem_writable()) {
        error("Root filesystem is not writable; cannot store the root password.", __FILE__);
        return -1;
    }

    int loaded = login_load_existing_accounts();
    if (loaded == 1)
        return 0;
    if (loaded < 0)
        return -1;

    loaded = login_migrate_legacy_account();
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

    bool saved = login_write_account_database(&record);
    login_clear_secret(password, sizeof(password));
    login_clear_secret(confirmation, sizeof(confirmation));
    if (!saved) {
        memset(&record, 0, sizeof(record));
        error("Could not persist root credentials; refusing login without them.", __FILE__);
        return -1;
    }

    loaded = login_load_existing_accounts();
    memset(&record, 0, sizeof(record));
    return loaded == 1 ? 0 : -1;
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
