/**
 * @file sh.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2025-01-14
 * 
 * @copyright Copyright (c) Pradosh 2025
 * 
 */

#include <sh_util.h>

int last_status_code = 0;

void init_command_list(command_list* lst)
{
    if (lst == NULL)
        return;

    memset(lst, 0, sizeof(command_list));
}

void dispose_command_list(command_list* lst)
{
    if (lst == NULL)
        return;

    for (command_list_entry* entry = lst->start; entry != NULL;)
    {
        command_list_entry* next = entry->next;
        kfree(entry->command);
        kfree(entry);
        entry = next;
    }
}

void push_command_to_list(command_list* lst, const char* value, size_t length)
{
    if (lst == NULL || value == NULL || length == 0)
        return;

    command_list_entry* entry = kmalloc(sizeof(command_list_entry));
    assert(entry != NULL, __FILE__, __LINE__);
    if (entry == NULL)
        return;

    memset(entry, 0, sizeof(command_list_entry));
    entry->length = length;
    entry->command = kmalloc(length+1);
    memset(entry->command, 0, length+1);
    memcpy(entry->command, value, length);

    if (lst->start == NULL)
    {
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

void welcome_message(){
    printf(blue_color " ______             _           _  _____ _          _ _ ");
    printf(blue_color "|  ____|           | |         | |/ ____| |        | | |");
    printf(blue_color "| |__ _ __ ___  ___| |_ ___  __| | (___ | |__   ___| | |");
    printf(blue_color "|  __| '__/ _ \\/ __| __/ _ \\/ _` |\\___ \\| '_ \\ / _ \\ | |");
    printf(blue_color "| |  | | | (_) \\__ \\ ||  __/ (_| |____) | | | |  __/ | |");
    printf(blue_color "|_|  |_|  \\___/|___/\\__\\___|\\__,_|_____/|_| |_|\\___|_|_|" reset_color "\n");

    printf(green_color "Welcome to frosted shell!" reset_color " This is an implementation of " blue_color "sh" reset_color ".");
    printf("We as the developers try to make this shell as similar as " blue_color "sh" reset_color ".\n");

    printf("Wiki    : " blue_color "https://github.com/Frost-Wing/osdev/wiki" reset_color "");
    printf("Github  : " blue_color "https://github.com/Frost-Wing" reset_color "\n");

    uint8_t second, minute, hour, day, month, year;
    update_system_time(&second, &minute, &hour, &day, &month, &year);

    printf("Time    : %02d:%02d:%02d %02d/%02d/%02d",
           hour, minute, second, day, month, year);
}

extern int64* wm_addr;

void start_window_manager(){
    void* file_addr = wm_addr;
    fdlfcn_handle* handle = fdlopen(file_addr, FDL_IMMEDIATE);
    int(*startfunction)(void);
    startfunction = (int(*)(void))fdlsym(FLD_NEXT, "_start");
    if (startfunction != NULL)
    {
        int result = startfunction();
        printf("Result function: %d", result);
        info("Successfully loaded function from .so file", __FILE__);
    }
    fdlclose(handle);
}

extern struct flanterm_context* ft_ctx;
struct fwrfs* global_fs;

int show_prompt(int argc, char** argv){

    if(last_status_code == 0)
        printfnoln("[" green_color "%d" reset_color "] ", last_status_code);
    else
        printfnoln("[" red_color "%d" reset_color "] ", last_status_code);

    print(vfs_getcwd());
    print(" @ ");

    if(strcmp(argv[0], "root") == 0){
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

void sh_exec(){
    int failed_attempts = 0;

    while(true){
        if (failed_attempts >= 5){
            error("You tried 5 diffrent wrong attempts. You've been locked out.", __FILE__);
            hcf2();
        }

        char* username = login_request();
        
        if(username != NULL){
            int argc = 1;

            int isSudo = 0;
            if(strcmp(username, "root") == 0)
                isSudo = 1;
            
            char* dummy_argv[] = {username, (char*)isSudo};
            shell_main(argc, dummy_argv);
        } else {
            error("Invalid credentials.", __FILE__);
            failed_attempts++;
        }
    }
}

int shell_main(int argc, char** argv){
    running = true;
    char* command = kmalloc(BUFFER_SIZE);
    size_t commandBufferSize = BUFFER_SIZE;
    size_t commandSize = 0;
    size_t cursor = 0;

    init_fs(global_fs);

    current_user = argv[0];

    done("Successfully logged in!", __FILE__);
    print("\x1b[2J\x1b[H");
    welcome_message();

    command_list commandHistory;
    init_command_list(&commandHistory);
    
    putc('\n');

    show_prompt(argc, argv);

    uint8_t commandPulledFromHistory = 0;
    command_list_entry* entry = NULL;
    char c;
    while (running)
    {
        c = getc();

        if (c == 0)
            continue;

        if (c == '\n')
        {
            command[cursor] = '\0'; // Null-terminate the string
            putc('\n');
            
            last_status_code = execute_chain(command);

            push_command_to_list(&commandHistory, command, cursor);
            cursor = 0;
            commandSize = 0;
            memset(command, 0, commandBufferSize);

            if(running){
                show_prompt(argc, argv);
            }
            continue;
        }
        else if (c == '\b')
        {
            if (cursor > 0)
                cursor--;
            else continue;
        }
        else
        {
            if (commandSize+1 >= commandBufferSize)
            {
                commandBufferSize += BUFFER_SIZE;
                command = krealloc(command, commandBufferSize);
            }

            command[cursor] = (char)c;
            cursor++;
            commandSize++;
        }

        putc(c);
    }
    kfree(command);

    dispose_command_list(&commandHistory);

    return 0;
}

typedef struct {
    vfs_file_t* file;   // NULL → terminal
} stream_impl_t;

extern stream_impl_t streams[];

static void apply_redirection(redir_t* r,
                              vfs_file_t** old_out,
                              vfs_file_t** old_err)
{
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
            *old_out = streams[STDOUT].file;

        stream_set_file(STDOUT, &file_handle);
    }

    /* ---------- STDERR ---------- */
    if (r->redirect_stderr) {
        if (old_err)
            *old_err = streams[STDERR].file;

        stream_set_file(STDERR, &file_handle);
    }
}
static void parse_redirection(int* argc, char** argv, redir_t* r)
{
    memset(r, 0, sizeof(redir_t));

    for(int i = 0; i < *argc; i++) {
        if(strcmp(argv[i], ">") == 0 && i+1 < *argc) {
            r->redirect_stdout = 1;
            r->append = 0;
            r->filename = argv[i+1];
        }
        else if(strcmp(argv[i], ">>") == 0 && i+1 < *argc) {
            r->redirect_stdout = 1;
            r->append = 1;
            r->filename = argv[i+1];
        }
        else if(strcmp(argv[i], "&>") == 0 && i+1 < *argc) {
            r->redirect_stdout = 1;
            r->redirect_stderr = 1;
            r->append = 0;
            r->filename = argv[i+1];
        }
        else {
            continue;
        }

        /* remove operator + filename from argv */
        for(int j = i; j+2 < *argc; j++)
            argv[j] = argv[j+2];

        *argc -= 2;
        argv[*argc] = NULL;
        i--;
    }
}

static void restore_redirection(vfs_file_t* old_out,
                                vfs_file_t* old_err)
{
    stream_set_file(STDOUT, old_out);
    stream_set_file(STDERR, old_err);
}


/* parse command line into sequence of subcmd_t.
   E.g. "a && b || c" => ["a"(OP_AND), "b"(OP_OR), "c"(OP_NONE)]
   Returns number of subcommands parsed.
   Caller must free each subcmd[i].cmd.
*/
static int parse_chain(const char* line, subcmd_t* out, int max_out)
{
    int count = 0;
    const char* p = line;
    const char* start = p;

    while(*p && count < max_out) {
        const char* q = p;
        bool in_squote = false, in_dquote = false;
        bool found_op = false;
        while(*q) {
            if(*q == '\'' && !in_dquote) in_squote = !in_squote;
            else if(*q == '"' && !in_squote) in_dquote = !in_dquote;
            else if(!in_squote && !in_dquote) {
                if(q[0] == '&' && q[1] == '&') { found_op = true; break; }
                if(q[0] == '|' && q[1] == '|') { found_op = true; break; }
            }
            q++;
        }

        size_t len;
        op_t op = OP_NONE;
        if(found_op) {
            len = (size_t)(q - p);
            if(q[0] == '&' && q[1] == '&') op = OP_AND;
            else if(q[0] == '|' && q[1] == '|') op = OP_OR;
        } else {
            len = strlen(p);
        }

        char* buf = (char*)kmalloc(len + 1);
        memcpy(buf, p, len);
        buf[len] = '\0';
        trim_inplace((char*)buf);

        out[count].cmd = buf;
        out[count].op_after = op;
        count++;

        if(found_op) {
            q += 2;
            while(*q && isspace((unsigned char)*q)) q++;
            p = q;
        } else {
            break;
        }
    }

    return count;
}

static command_t commands[] = {
    { "echo", cmd_echo },
    { "touch", cmd_touch },
    { "rm", cmd_rm },
    { "mkdir", cmd_mkdir },
    { "cat", cmd_cat },
    { "ls", cmd_ls },
    { "clear", cmd_clear },
    { "pwd", cmd_pwd },
    { "cd", cmd_cd },
    { "whoami", cmd_whoami },
    { "shutdown", cmd_shutdown },
    { "lspci", cmd_lspci },
    { "lsblk", cmd_lsblk },
    { "mount", cmd_mount },
    { "mv", cmd_mv },
    { "umount", cmd_umount }
    // { "fwfetch", cmd_fwfetch },
    // { "help", cmd_help },
};


static int dispatch(int argc, char** argv)
{
    if(argc == 0) return 0;
    const char* cmd = argv[0];

    for(size_t i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
        if(strcmp(cmd, commands[i].name) == 0) {
            return commands[i].func(argc, argv);
        }
    }

    printf("fsh: %s: not found", cmd);
    return 127;
}


int execute_chain(const char* line)
{
    if(!line) return 0;
    char tmp[MAX_COMMAND_LINE];
    strncpy(tmp, line, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    trim_inplace(tmp);
    if(tmp[0] == '\0') return 0;

    subcmd_t parts[MAX_SUBCOMMANDS];
    int n = parse_chain(tmp, parts, MAX_SUBCOMMANDS);
    int last_status = 0;

    for(int i = 0; i < n; ++i) {
        /* If previous operator was && and last_status != 0 then skip current */
        if(i > 0) {
            op_t prevop = parts[i-1].op_after;
            if(prevop == OP_AND && last_status != 0) {
                /* skip execution */
                last_status = last_status; /* unchanged */
                continue;
            } else if(prevop == OP_OR && last_status == 0) {
                /* skip execution because prior succeeded */
                continue;
            }
        }

        /* Tokenize current subcommand into argv */
        char* argv[MAX_ARGV];
        int argc = split_args(parts[i].cmd, argv, MAX_ARGV);

        if(argc == 0) {
            for(int k=0;k<argc;k++)
                kfree(argv[k]);
            continue;
        }

        redir_t redir;
        parse_redirection(&argc, argv, &redir);

        vfs_file_t* old_out = NULL;
        vfs_file_t* old_err = NULL;

        apply_redirection(&redir, &old_out, &old_err);

        last_status = dispatch(argc, argv);

        restore_redirection(NULL, NULL);

        for(int k=0;k<argc;k++)
            kfree(argv[k]);
    }

    for(int i=0;i<n;i++)
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
int split_args(const char* cmdline, char** argv, int max_args)
{
    int argc = 0;
    const char* p = cmdline;
    while(*p && argc < max_args) {
        while(*p && isspace((unsigned char)*p)) p++;
        if(!*p) break;

        char quote = 0;
        if(*p == '"' || *p == '\'') {
            quote = *p;
            p++;
        }

        const char* start = p;
        size_t bufcap = 128;
        char* buf = (char*)kmalloc(bufcap);
        size_t len = 0;

        if(quote) {
            while(*p && *p != quote) {
                if(len + 1 >= bufcap) { bufcap *= 2; buf = krealloc(buf, bufcap); }
                buf[len++] = *p++;
            }
            if(*p == quote) p++; /* skip ending quote */
        } else {
            while(*p && !isspace((unsigned char)*p)) {
                if(len + 1 >= bufcap) { bufcap *= 2; buf = krealloc(buf, bufcap); }
                buf[len++] = *p++;
            }
        }
        buf[len] = '\0';
        argv[argc++] = buf;
    }

    return argc;
}