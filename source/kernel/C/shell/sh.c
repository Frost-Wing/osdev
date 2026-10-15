/**
 * @file sh.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-01-14
 *
 * @copyright Copyright (c) Pradosh 2025-2026
 *
 */

#include <commands/login.h>
#include <filesystems/vfs.h>
#include <graphics.h>
#include <multitasking.h>
#include <sh_util.h>
#include <strings.h>
#include <cc-asm.h>
#include <rtc.h>
#include <tty.h>
#include <stream.h>
#include <sys/dirent.h>
#include <syscalls/internal.h>

int last_status_code = 0;
string current_user;

static char global_home_env[64] = "HOME=/";
static char global_path_env[256] = "PATH=/bin:/usr/bin:/sbin:/usr/sbin";
static char global_term_env[64] = "TERM=linux";
static char global_user_env[64] = "USER=none";
static char global_shlvl_env[32] = "SHLVL=1";

char *global_envp[] = {
    global_home_env,
    global_path_env,
    global_term_env,
    global_user_env,
    global_shlvl_env,
    NULL};

void init_command_list(command_list *lst) {
    if (lst == NULL)
        return;

    memset(lst, 0, sizeof(command_list));
}

void dispose_command_list(command_list *lst) {
    if (lst == NULL)
        return;

    for (command_list_entry *entry = lst->start; entry != NULL;) {
        command_list_entry *next = entry->next;
        kfree(entry->command);
        kfree(entry);
        entry = next;
    }
}

void push_command_to_list(command_list *lst, const char *value, size_t length) {
    if (lst == NULL || value == NULL || length == 0)
        return;

    command_list_entry *entry = kmalloc(sizeof(command_list_entry));
    assert(entry != NULL);
    if (entry == NULL)
        return;

    memset(entry, 0, sizeof(command_list_entry));
    entry->length = length;
    entry->command = kmalloc(length + 1);
    memset(entry->command, 0, length + 1);
    memcpy(entry->command, value, length);

    if (lst->start == NULL) {
        lst->start = lst->end = entry;
        lst->count++;
        return;
    }

    entry->prev = lst->end;
    lst->end->next = entry;
    lst->end = entry;
    lst->count++;
}

bool running = true;

void welcome_message(void) {
    printf(blue_color " ______             _           _  _____ _          _ _ ");
    printf(blue_color "|  ____|           | |         | |/ ____| |        | | |");
    printf(blue_color "| |__ _ __ ___  ___| |_ ___  __| | (___ | |__   ___| | |");
    printf(blue_color "|  __| '__/ _ \\/ __| __/ _ \\/ _` |\\___ \\| '_ \\ / _ \\ | |");
    printf(blue_color "| |  | | | (_) \\__ \\ ||  __/ (_| |____) | | | |  __/ | |");
    printf(blue_color "|_|  |_|  \\___/|___/\\__\\___|\\__,_|_____/|_| |_|\\___|_|_|" reset_color "\n");

    printf(green_color "Welcome to frosted shell!" reset_color " This is an custom implementation of " blue_color "sh" reset_color ".");

    printf("Wiki    : " blue_color "https://github.com/Frost-Wing/osdev/wiki" reset_color "");
    printf("Github  : " blue_color "https://github.com/Frost-Wing" reset_color "\n");

    printf(blue_color "This shell is under kernel mode (ring-0) you are responsible for your actions.");
    printf(blue_color "Root (/) and other VFS are not mounted, you need to mount them by youself. Check out" red_color " wiki " blue_color "for more details.\n" reset_color);

    uint8_t second, minute, hour, day, month;
    uint16_t year;
    update_system_time(&second, &minute, &hour, &day, &month, &year);

    printf("Time    : %02d:%02d:%02d %02d/%02d/%02d",
        hour, minute, second, day, month, year);
}

extern uint64 *wm_addr;

void start_window_manager(void) {
    void *file_addr = wm_addr;
    fdlfcn_handle *handle = fdlopen(file_addr, FDL_IMMEDIATE);
    int (*startfunction)(void);
    startfunction = (int (*)(void))fdlsym(FLD_NEXT, "_start");
    if (startfunction != NULL) {
        int result = startfunction();
        printf("Result function: %d", result);
        info("Successfully loaded function from .so file", __FILE__);
    }
    fdlclose(handle);
}

extern struct flanterm_context *ft_ctx;
struct fwrfs *global_fs;

int show_prompt(int argc, char **argv) {
    (void)argc;

    if (last_status_code == 0)
        printfnoln("[" green_color "%d" reset_color "] ", (int64_t)last_status_code);
    else
        printfnoln("[" red_color "%d" reset_color "] ", (int64_t)last_status_code);

    print(vfs_getcwd());
    print(" @ ");

    if (strcmp(argv[0], "root") == 0) {
        print(red_color);
        print(argv[0]);
        print(reset_color);
    } else {
        print(green_color);
        print(argv[0]);
        print(reset_color);
    }

    print(" $ ");

    return 0;
}

void ksh_exec(void) {
    if (login_setup_first_user() != 0) {
        error("No account was configured; refusing interactive login.", __FILE__);
        hcf2();
    }

    int failed_attempts = 0;

    while (true) {
        if (failed_attempts >= 5) {
            error("You tried 5 different wrong attempts. You've been locked out.", __FILE__);
            hcf2();
        }

        char username[32];
        int result = login_request(username, sizeof(username));

        if (result == 0) {
            int argc = 1;

            int isSudo = 0;
            if (strcmp(username, "root") == 0)
                isSudo = 1;

            /* safer argument passing */
            char sudo_flag = isSudo;

            char *dummy_argv[] = {
                username,
                &sudo_flag};

            snprintf(global_user_env, sizeof(global_user_env), "USER=%s", username);

            shell_main(argc, dummy_argv);
        } else {
            error("Invalid credentials.", __FILE__);
            failed_attempts++;
        }
    }
}

static void shell_set_echo(bool enabled) {
    linux_termios_t termios;
    if (!tty_get_termios(&termios))
        return;

    if (enabled)
        termios.c_lflag |= LINUX_ECHO;
    else
        termios.c_lflag &= (uint32_t)~LINUX_ECHO;

    tty_set_termios(&termios, false);
}

static void shell_move_cursor(size_t from, size_t to) {
    const char *sequence = from > to ? "\033[D" : "\033[C";
    size_t count = from > to ? from - to : to - from;
    while (count--)
        print(sequence);
}

static char shell_ghost[256];
static size_t shell_display_size;
static bool shell_completion_suggestion(const char *command, size_t size,
    size_t cursor, char *out,
    size_t out_size);

static void shell_redraw_line(const char *command, size_t old_size,
    size_t old_cursor, size_t new_size,
    size_t new_cursor) {
    (void)old_size;
    shell_move_cursor(old_cursor, 0);
    for (size_t i = 0; i < new_size; ++i)
        putc(command[i]);

    shell_ghost[0] = '\0';
    shell_completion_suggestion(command, new_size, new_cursor,
        shell_ghost, sizeof(shell_ghost));
    size_t ghost_size = strlen(shell_ghost);
    if (ghost_size != 0) {
        print("\x1b[90m");
        print(shell_ghost);
        print(reset_color);
    }

    size_t new_display_size = new_size + ghost_size;
    size_t display_size = shell_display_size > new_display_size
                              ? shell_display_size
                              : new_display_size;
    for (size_t i = new_display_size; i < display_size; ++i)
        putc(' ');
    shell_move_cursor(display_size, new_cursor);
    shell_display_size = new_display_size;
}

static void shell_clear_line(size_t size, size_t cursor) {
    (void)size;
    shell_move_cursor(cursor, 0);
    for (size_t i = 0; i < shell_display_size; ++i)
        putc(' ');
    shell_move_cursor(shell_display_size, 0);
    shell_display_size = 0;
    shell_ghost[0] = '\0';
}

static size_t shell_complete_command(char *command, size_t *command_size,
    size_t capacity);
static void shell_print_command_matches(void);

int shell_main(int argc, char **argv) {
    running = true;
    char *command = kmalloc(BUFFER_SIZE);
    size_t commandBufferSize = BUFFER_SIZE;
    size_t commandSize = 0;
    size_t cursor = 0;

    current_user = argv[0];

    done("Successfully logged in!", __FILE__);
    print("\x1b[2J\x1b[H");
    welcome_message();

    command_list commandHistory;
    init_command_list(&commandHistory);

    putc('\n');

    show_prompt(argc, argv);
    shell_set_echo(false);
    shell_display_size = 0;
    shell_ghost[0] = '\0';

    command_list_entry *entry = NULL;
    int c;
    while (running) {
        multitasking_pump();
        c = getc();

        if (c == 0)
            continue;

        if (c == '\t') {
            if (cursor == commandSize) {
                size_t old_cursor = cursor;
                size_t old_size = commandSize;
                size_t matches = shell_complete_command(
                    command, &commandSize, commandBufferSize);
                cursor = commandSize;

                if (commandSize != old_size) {
                    shell_redraw_line(command, old_size, old_cursor,
                        commandSize, cursor);
                } else if (matches > 0) {
                    size_t ghost_size = strlen(shell_ghost);
                    for (size_t i = 0; i < ghost_size; ++i)
                        putc(' ');
                    shell_move_cursor(ghost_size, 0);
                    shell_ghost[0] = '\0';
                    shell_display_size = commandSize;
                    putc('\n');
                    shell_print_command_matches();
                    putc('\n');
                    show_prompt(argc, argv);
                    shell_display_size = 0;
                    shell_redraw_line(command, 0, 0, commandSize, cursor);
                }
            }
            continue;
        }

        if (c == 3 || c == 4) {
            tty_take_interrupt();
            tty_flush_input();
            keyboard_flush_buffer();
            shell_clear_line(commandSize, cursor);
            commandSize = 0;
            cursor = 0;
            command[0] = '\0';
            if (c == 3)
                print("^C\n");
            else
                putc('\n');
            shell_display_size = 0;
            shell_ghost[0] = '\0';
            entry = NULL;
            show_prompt(argc, argv);
            shell_set_echo(false);
            shell_display_size = 0;
            continue;
        }

        if (c == CUR_UP) {
            command_list_entry *previous_entry = entry;
            if (entry == NULL)
                entry = commandHistory.end;
            else if (entry->prev != NULL)
                entry = entry->prev;

            if (entry != NULL) {
                size_t old_size = commandSize;
                size_t old_cursor = cursor;
                commandSize = entry->length;
                cursor = entry->length;
                memcpy(command, entry->command, commandSize);
                command[commandSize] = '\0';
                shell_redraw_line(command, old_size, old_cursor,
                    commandSize, cursor);
            } else {
                entry = previous_entry;
            }
            continue;
        }

        if (c == CUR_DOWN) {
            if (entry != NULL)
                entry = entry->next;

            size_t old_size = commandSize;
            size_t old_cursor = cursor;
            commandSize = 0;
            cursor = 0;
            command[0] = '\0';

            if (entry != NULL) {
                commandSize = entry->length;
                cursor = entry->length;
                memcpy(command, entry->command, commandSize);
                command[commandSize] = '\0';
            }
            shell_redraw_line(command, old_size, old_cursor,
                commandSize, cursor);
            continue;
        }

        if (c == '\n') {
            shell_move_cursor(cursor, commandSize);
            size_t ghost_size = strlen(shell_ghost);
            for (size_t i = 0; i < ghost_size; ++i)
                putc(' ');
            shell_move_cursor(ghost_size, 0);
            shell_ghost[0] = '\0';
            shell_display_size = commandSize;
            putc('\n');
            command[commandSize] = '\0';

            tty_flush_input();
            shell_set_echo(true);
            last_status_code = execute_chain(command);
            int interrupt_exit_code = tty_take_interrupt();
            if (interrupt_exit_code >= 0) {
                keyboard_flush_buffer();
                tty_flush_input();
                last_status_code = interrupt_exit_code;
            }

            push_command_to_list(&commandHistory, command, commandSize);
            entry = NULL;
            cursor = 0;
            commandSize = 0;
            memset(command, 0, commandBufferSize);
            shell_display_size = 0;
            shell_ghost[0] = '\0';

            if (running) {
                shell_set_echo(false);
                show_prompt(argc, argv);
            }
            continue;
        }

        if (c == CUR_LEFT) {
            if (cursor > 0) {
                size_t old_cursor = cursor;
                cursor--;
                shell_redraw_line(command, commandSize, old_cursor,
                    commandSize, cursor);
            }
            continue;
        }

        if (c == CUR_RIGHT) {
            if (cursor < commandSize) {
                size_t old_cursor = cursor;
                cursor++;
                shell_redraw_line(command, commandSize, old_cursor,
                    commandSize, cursor);
            }
            continue;
        }

        if (c == KEY_HOME) {
            size_t old_cursor = cursor;
            cursor = 0;
            shell_redraw_line(command, commandSize, old_cursor,
                commandSize, cursor);
            continue;
        }

        if (c == KEY_END) {
            size_t old_cursor = cursor;
            cursor = commandSize;
            shell_redraw_line(command, commandSize, old_cursor,
                commandSize, cursor);
            continue;
        }

        if (c == '\b' || c == 127) {
            if (cursor == 0)
                continue;

            size_t old_size = commandSize;
            size_t old_cursor = cursor;
            memmove(command + cursor - 1, command + cursor,
                commandSize - cursor + 1);
            commandSize--;
            cursor--;
            shell_redraw_line(command, old_size, old_cursor,
                commandSize, cursor);
            continue;
        }

        if (c == KEY_DEL) {
            if (cursor >= commandSize)
                continue;

            size_t old_size = commandSize;
            memmove(command + cursor, command + cursor + 1,
                commandSize - cursor);
            commandSize--;
            shell_redraw_line(command, old_size, cursor,
                commandSize, cursor);
            continue;
        }

        if (c < 32 || c > 126)
            continue;

        if (commandSize + 1 >= commandBufferSize) {
            commandBufferSize += BUFFER_SIZE;
            command = krealloc(command, commandBufferSize);
        }

        size_t old_cursor = cursor;
        memmove(command + cursor + 1, command + cursor,
            commandSize - cursor + 1);
        command[cursor] = (char)c;
        commandSize++;
        cursor++;
        shell_redraw_line(command, commandSize - 1, old_cursor,
            commandSize, cursor);
    }
    kfree(command);

    dispose_command_list(&commandHistory);

    return 0;
}

static void apply_redirection(redir_t *r,
    vfs_file_t **old_out,
    vfs_file_t **old_err) {
    if (!r || (!r->redirect_stdout && !r->redirect_stderr))
        return;

    static vfs_file_t file_handle;

    int flags = VFS_WRONLY | VFS_CREATE;

    if (r->append)
        flags |= VFS_APPEND;
    else
        flags |= VFS_TRUNC;

    int ret = vfs_open(r->filename, flags, &file_handle);
    if (ret < 0) {
        printf("fsh: cannot open %s", r->filename);
        return;
    }

    /* ---------- STDOUT ---------- */
    if (r->redirect_stdout) {
        if (old_out)
            *old_out = stream_get_file(STDOUT);

        stream_set_file(STDOUT, &file_handle);
    }

    /* ---------- STDERR ---------- */
    if (r->redirect_stderr) {
        if (old_err)
            *old_err = stream_get_file(STDERR);

        stream_set_file(STDERR, &file_handle);
    }
}

static void parse_redirection(int *argc, char **argv, redir_t *r) {
    memset(r, 0, sizeof(redir_t));

    for (int i = 0; i < *argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < *argc) {
            r->redirect_stdout = 1;
            r->append = 0;
            r->filename = argv[i + 1];
        } else if (strcmp(argv[i], ">>") == 0 && i + 1 < *argc) {
            r->redirect_stdout = 1;
            r->append = 1;
            r->filename = argv[i + 1];
        } else if (strcmp(argv[i], "&>") == 0 && i + 1 < *argc) {
            r->redirect_stdout = 1;
            r->redirect_stderr = 1;
            r->append = 0;
            r->filename = argv[i + 1];
        } else {
            continue;
        }

        /* remove operator + filename from argv */
        for (int j = i; j + 2 < *argc; j++)
            argv[j] = argv[j + 2];

        *argc -= 2;
        argv[*argc] = NULL;
        i--;
    }
}

static void restore_redirection(vfs_file_t *old_out,
    vfs_file_t *old_err) {
    stream_set_file(STDOUT, old_out);
    stream_set_file(STDERR, old_err);
}

/* parse command line into sequence of subcmd_t.
   E.g. "a && b || c" => ["a"(OP_AND), "b"(OP_OR), "c"(OP_NONE)]
   Returns number of subcommands parsed.
   Caller must free each subcmd[i].cmd.
*/
static int parse_chain(const char *line, subcmd_t *out, int max_out) {
    int count = 0;
    const char *p = line;

    while (*p && count < max_out) {
        const char *q = p;
        bool in_squote = false, in_dquote = false;
        bool found_op = false;
        while (*q) {
            if (*q == '\'' && !in_dquote)
                in_squote = !in_squote;
            else if (*q == '"' && !in_squote)
                in_dquote = !in_dquote;
            else if (!in_squote && !in_dquote) {
                if (q[0] == '&' && q[1] == '&') {
                    found_op = true;
                    break;
                }
                if (q[0] == '|' && q[1] == '|') {
                    found_op = true;
                    break;
                }
            }
            q++;
        }

        size_t len;
        op_t op = OP_NONE;
        if (found_op) {
            len = (size_t)(q - p);
            if (q[0] == '&' && q[1] == '&')
                op = OP_AND;
            else if (q[0] == '|' && q[1] == '|')
                op = OP_OR;
        } else {
            len = strlen(p);
        }

        char *buf = (char *)kmalloc(len + 1);
        memcpy(buf, p, len);
        buf[len] = '\0';
        trim_inplace((char *)buf);

        out[count].cmd = buf;
        out[count].op_after = op;
        count++;

        if (found_op) {
            q += 2;
            while (*q && isspace((unsigned char)*q))
                q++;
            p = q;
        } else {
            break;
        }
    }

    return count;
}

static command_t commands[] = {
    {"echo", cmd_echo},
    {"touch", cmd_touch},
    {"rm", cmd_rm},
    {"mkdir", cmd_mkdir},
    {"cat", cmd_cat},
    {"ls", cmd_ls},
    {"clear", cmd_clear},
    {"pwd", cmd_pwd},
    {"cd", cmd_cd},
    {"whoami", cmd_whoami},
    {"shutdown", cmd_shutdown},
    {"lspci", cmd_lspci},
    {"lsblk", cmd_lsblk},
    {"lsusb", cmd_lsusb},
    {"mount", cmd_mount},
    {"mv", cmd_mv},
    {"umount", cmd_umount},
    {"exec", cmd_exec},
    {"tasks", cmd_tasks},
    {"ping", cmd_ping},
    {"wget", cmd_wget},
    {"cp", cmd_cp},
    // { "fwfetch", cmd_fwfetch },
    // { "help", cmd_help },
};

enum {
    SHELL_COMPLETION_LIST_MAX = 64,
    SHELL_COMPLETION_NAME_MAX = 256,
    SHELL_COMPLETION_READ_SIZE = 2048
};

typedef struct {
    size_t matches;
    size_t listed;
    size_t common_size;
    char first_name[SHELL_COMPLETION_NAME_MAX];
    char common_name[SHELL_COMPLETION_NAME_MAX];
    bool first_is_dir;
} shell_completion_t;

static char shell_completion_list[SHELL_COMPLETION_LIST_MAX]
                                 [SHELL_COMPLETION_NAME_MAX];
static bool shell_completion_list_is_dir[SHELL_COMPLETION_LIST_MAX];
static size_t shell_completion_list_count;

static void shell_completion_add(shell_completion_t *result,
    const char *name, bool is_dir,
    const char *prefix, size_t prefix_size) {
    size_t name_size = strlen(name);
    if (name_size == 0 || name_size >= SHELL_COMPLETION_NAME_MAX ||
        name_size < prefix_size || strncmp(name, prefix, prefix_size) != 0)
        return;

    if (result->matches == 0) {
        memcpy(result->first_name, name, name_size + 1);
        memcpy(result->common_name, name, name_size + 1);
        result->common_size = name_size;
        result->first_is_dir = is_dir;
    } else {
        size_t limit = result->common_size < name_size
                           ? result->common_size
                           : name_size;
        size_t i = 0;
        while (i < limit && result->common_name[i] == name[i])
            ++i;
        result->common_size = i;
    }

    if (result->listed < SHELL_COMPLETION_LIST_MAX) {
        size_t index = result->listed++;
        memcpy(shell_completion_list[index], name, name_size + 1);
        shell_completion_list_is_dir[index] = is_dir;
    }
    result->matches++;
}

static size_t shell_token_start(const char *command, size_t size) {
    size_t start = size;
    while (start > 0 && command[start - 1] != ' ' &&
           command[start - 1] != '\t')
        --start;
    return start;
}

static void shell_completion_collect(const char *command, size_t size,
    size_t start, shell_completion_t *result,
    size_t *directory_size,
    size_t *base_size, bool *command_position) {
    memset(result, 0, sizeof(*result));
    shell_completion_list_count = 0;
    *directory_size = 0;
    *base_size = size - start;
    size_t segment_start = 0;
    bool in_single_quote = false;
    bool in_double_quote = false;
    for (size_t i = 0; i < start; ++i) {
        if (command[i] == '\'' && !in_double_quote)
            in_single_quote = !in_single_quote;
        else if (command[i] == '"' && !in_single_quote)
            in_double_quote = !in_double_quote;
        else if (!in_single_quote && !in_double_quote && i + 1 < start &&
                 ((command[i] == '&' && command[i + 1] == '&') ||
                     (command[i] == '|' && command[i + 1] == '|'))) {
            segment_start = i + 2;
            ++i;
        }
    }
    *command_position = true;
    for (size_t i = segment_start; i < start; ++i) {
        if (command[i] != ' ' && command[i] != '\t') {
            *command_position = false;
            break;
        }
    }

    if (*command_position) {
        for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i)
            shell_completion_add(result, commands[i].name, false,
                command + start, *base_size);
        shell_completion_list_count = result->listed;
        return;
    }

    const char *token = command + start;
    for (size_t i = 0; i < *base_size; ++i) {
        if (token[i] == '/')
            *directory_size = i + 1;
    }

    const char *base = token + *directory_size;
    size_t base_length = *base_size - *directory_size;
    char directory[SHELL_COMPLETION_NAME_MAX];
    if (*directory_size >= sizeof(directory))
        return;

    if (*directory_size == 0) {
        directory[0] = '.';
        directory[1] = '\0';
    } else {
        memcpy(directory, token, *directory_size);
        directory[*directory_size] = '\0';
    }

    int fd = fd_open(directory, VFS_RDONLY);
    if (fd < 0)
        return;

    char buffer[SHELL_COMPLETION_READ_SIZE];
    for (;;) {
        int bytes = sys_getdents64((uint64_t)fd, buffer, sizeof(buffer));
        if (bytes <= 0)
            break;

        size_t offset = 0;
        while (offset + sizeof(linux_dirent64_t) <= (size_t)bytes) {
            linux_dirent64_t *entry = (linux_dirent64_t *)(buffer + offset);
            size_t record_size = entry->d_reclen;
            if (record_size < sizeof(linux_dirent64_t) + 1 ||
                record_size > (size_t)bytes - offset)
                break;

            size_t name_capacity = record_size - sizeof(linux_dirent64_t);
            size_t name_size = 0;
            while (name_size < name_capacity && entry->d_name[name_size] != '\0')
                ++name_size;
            if (name_size < name_capacity &&
                name_size < SHELL_COMPLETION_NAME_MAX) {
                shell_completion_add(result, entry->d_name,
                    entry->d_type == 4, base, base_length);
            }
            offset += record_size;
        }
    }
    fd_close(fd);
    shell_completion_list_count = result->listed;
}

static size_t shell_complete_command(char *command, size_t *command_size,
    size_t capacity) {
    size_t start = shell_token_start(command, *command_size);
    size_t directory_size, base_size;
    bool command_position;
    shell_completion_t result;
    shell_completion_collect(command, *command_size, start, &result,
        &directory_size, &base_size, &command_position);
    if (result.matches == 0)
        return 0;

    size_t completed_base_size = result.matches == 1
                                     ? (size_t)strlen(result.first_name)
                                     : result.common_size;
    bool unique = result.matches == 1;
    char suffix = unique
                      ? (command_position || !result.first_is_dir ? ' ' : '/')
                      : '\0';
    size_t new_size = start + directory_size + completed_base_size +
                      (suffix ? 1 : 0);
    if (new_size >= capacity)
        return result.matches;

    size_t current_base_size = base_size - directory_size;
    if (completed_base_size > current_base_size) {
        memcpy(command + *command_size,
            result.matches == 1 ? result.first_name + current_base_size
                                : result.common_name + current_base_size,
            completed_base_size - current_base_size);
    }
    if (suffix)
        command[new_size - 1] = suffix;

    if (new_size != *command_size || suffix) {
        *command_size = new_size;
        command[new_size] = '\0';
    }
    return result.matches;
}

static bool shell_completion_suggestion(const char *command, size_t size,
    size_t cursor, char *out,
    size_t out_size) {
    if (cursor != size || out_size == 0)
        return false;

    size_t start = shell_token_start(command, size);
    size_t directory_size, base_size;
    bool command_position;
    shell_completion_t result;
    shell_completion_collect(command, size, start, &result,
        &directory_size, &base_size, &command_position);
    if (result.matches != 1)
        return false;

    size_t name_size = strlen(result.first_name);
    size_t typed_base_size = base_size - directory_size;
    size_t suffix_size = name_size - typed_base_size;
    if (suffix_size + 2 > out_size)
        return false;
    memcpy(out, result.first_name + typed_base_size, suffix_size);
    out[suffix_size++] = command_position || !result.first_is_dir ? ' ' : '/';
    out[suffix_size] = '\0';
    return true;
}

static void shell_print_command_matches(void) {
    for (size_t i = 0; i < shell_completion_list_count; ++i) {
        print(shell_completion_list[i]);
        if (shell_completion_list_is_dir[i])
            putc('/');
        print("  ");
    }
}

static int dispatch(int argc, char **argv) {
    if (argc == 0)
        return 0;
    const char *cmd = argv[0];

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(cmd, commands[i].name) == 0) {
            return commands[i].func(argc, argv);
        }
    }

    printf("fsh: %s: not found", cmd);
    return 127;
}

int execute_chain(const char *line) {
    if (!line)
        return 0;
    char tmp[MAX_COMMAND_LINE];
    strncpy(tmp, line, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    trim_inplace(tmp);
    if (tmp[0] == '\0')
        return 0;

    subcmd_t parts[MAX_SUBCOMMANDS];
    int n = parse_chain(tmp, parts, MAX_SUBCOMMANDS);
    int last_status = 0;

    for (int i = 0; i < n; ++i) {
        /* If previous operator was && and last_status != 0 then skip current */
        if (i > 0) {
            op_t prevop = parts[i - 1].op_after;
            if (prevop == OP_AND && last_status != 0) {
                /* skip execution */
                continue;
            } else if (prevop == OP_OR && last_status == 0) {
                /* skip execution because prior succeeded */
                continue;
            }
        }

        /* Tokenize current subcommand into argv */
        char *argv[MAX_ARGV];
        int argc = split_args(parts[i].cmd, argv, MAX_ARGV);

        if (argc == 0) {
            for (int k = 0; k < argc; k++)
                kfree(argv[k]);
            continue;
        }

        redir_t redir;
        parse_redirection(&argc, argv, &redir);

        vfs_file_t *old_out = NULL;
        vfs_file_t *old_err = NULL;

        apply_redirection(&redir, &old_out, &old_err);

        last_status = dispatch(argc, argv);
        multitasking_pump();

        restore_redirection(NULL, NULL);

        for (int k = 0; k < argc; k++)
            kfree(argv[k]);
    }

    for (int i = 0; i < n; i++)
        kfree(parts[i].cmd);

    return last_status;
}

/*
 split_args:
  - tokenizes cmdline into argv[] up to max_args
  - respects single and double quotes ("..." and '...')
  - returns argc
  - argv[] are newly allocated strings (kmalloc). Caller must free each argv[i].
*/
int split_args(const char *cmdline, char **argv, int max_args) {
    int argc = 0;
    const char *p = cmdline;
    while (*p && argc < max_args) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;

        char quote = 0;
        if (*p == '"' || *p == '\'') {
            quote = *p;
            p++;
        }

        size_t bufcap = 128;
        char *buf = (char *)kmalloc(bufcap);
        size_t len = 0;

        if (quote) {
            while (*p && *p != quote) {
                if (len + 1 >= bufcap) {
                    bufcap *= 2;
                    buf = krealloc(buf, bufcap);
                }
                buf[len++] = *p++;
            }
            if (*p == quote)
                p++; /* skip ending quote */
        } else {
            while (*p && !isspace((unsigned char)*p)) {
                if (len + 1 >= bufcap) {
                    bufcap *= 2;
                    buf = krealloc(buf, bufcap);
                }
                buf[len++] = *p++;
            }
        }
        buf[len] = '\0';
        argv[argc++] = buf;
    }

    return argc;
}
